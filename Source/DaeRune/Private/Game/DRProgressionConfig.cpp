// Copyright DaeRune


#include "Game/DRProgressionConfig.h"
#include "Game/DRChipCatalog.h"
#include "DaeRune/DRLogChannels.h"
// Plan5(공속칩) — 공속 버프 칩 GE 검사
#include "DRGameplayTags.h"
#include "GameplayEffect.h"

// ========================= FDRJackpotOdds (Plan5(잭팟칩)) =========================

FDRJackpotOdds FDRJackpotOdds::Normalized(const FDRJackpotOdds& Fallback) const
{
	FDRJackpotOdds Out(FMath::Max(0.f, Bronze), FMath::Max(0.f, Silver), FMath::Max(0.f, Gold));

	const float Total = Out.Sum();
	if (Total <= KINDA_SMALL_NUMBER)
	{
		return Fallback;
	}

	Out.Bronze /= Total;
	Out.Silver /= Total;
	Out.Gold /= Total;
	return Out;
}

int32 UDRProgressionConfig::GetMaxSlotCount() const
{
	return FMath::Max(0, MaxSlots);
}

int32 UDRProgressionConfig::GetSlotUnlockCost(int32 SlotNumber) const
{
	if (SlotNumber <= 0 || SlotNumber > GetMaxSlotCount())
	{
		return -1;
	}

	if (SlotCosts.IsValidIndex(SlotNumber - 1))
	{
		return FMath::Max(0, SlotCosts[SlotNumber - 1]);
	}

	// 비용 미지정 시 선형 증가 폴백
	return FMath::Max(0, DefaultSlotCost * SlotNumber);
}

const FDRStageRewardDef* UDRProgressionConfig::FindStageReward(FName StageId) const
{
	if (StageId.IsNone()) return nullptr;

	return StageRewards.FindByPredicate(
		[StageId](const FDRStageRewardDef& Def) { return Def.StageId == StageId; });
}

const FDRAchievementDef* UDRProgressionConfig::FindAchievement(FName AchievementId) const
{
	if (AchievementId.IsNone()) return nullptr;

	return Achievements.FindByPredicate(
		[AchievementId](const FDRAchievementDef& Def) { return Def.AchievementId == AchievementId; });
}

FName UDRProgressionConfig::MakeStageClearRewardId(FName StageId)
{
	if (StageId.IsNone()) return NAME_None;

	return FName(*FString::Printf(TEXT("StageClear.%s"), *StageId.ToString()));
}

// ========================= 예산 검증 =========================

int32 UDRProgressionConfig::GetTotalCurrencyBudget() const
{
	int32 Total = 0;

	for (const FDRStageRewardDef& Def : StageRewards)
	{
		Total += FMath::Max(0, Def.FirstClearCurrency);
	}
	for (const FDRAchievementDef& Def : Achievements)
	{
		Total += FMath::Max(0, Def.Currency);
	}

	return Total;
}

int32 UDRProgressionConfig::GetSlotUnlockCostForClass() const
{
	int32 Total = 0;

	const int32 SlotCount = GetMaxSlotCount();

	// 무료 시작 슬롯은 재화를 쓰지 않으므로 필요 예산에서 제외한다 (Plan3(공용칩) §4.5).
	// 빼지 않으면 "전체 해금이 불가능하다"는 거짓 경고가 뜬다.
	const int32 FirstPaidSlot = FMath::Clamp(InitialFreeSlots, 0, SlotCount) + 1;

	for (int32 SlotNumber = FirstPaidSlot; SlotNumber <= SlotCount; ++SlotNumber)
	{
		Total += FMath::Max(0, GetSlotUnlockCost(SlotNumber));
	}

	return Total;
}

int32 UDRProgressionConfig::GetTotalSlotUnlockCost() const
{
	// 로봇 수는 하드코딩하지 않는다 — 클래스 추가에 자동 대응
	const int32 ClassCount = static_cast<int32>(EPlayerCharacterClass::Count);
	return GetSlotUnlockCostForClass() * ClassCount;
}

bool UDRProgressionConfig::ValidateCurrencyBudget(TArray<FString>& OutErrors) const
{
	OutErrors.Reset();

	const int32 Budget = GetTotalCurrencyBudget();
	const int32 Required = GetTotalSlotUnlockCost();

	if (Budget < Required)
	{
		OutErrors.Add(FString::Printf(
			TEXT("재화 부족: 획득 가능 총량 %d < 전체 슬롯 해금 비용 %d (부족 %d). ")
			TEXT("슬롯 비용을 낮추거나 스테이지/업적 보상을 늘려야 특정 로봇을 포기하지 않아도 됩니다."),
			Budget, Required, Required - Budget));
	}

	// 중복 키 검사 (원장이 FName 단일 키라 충돌하면 한쪽이 조용히 무효가 된다)
	TSet<FName> SeenIds;
	for (const FDRStageRewardDef& Def : StageRewards)
	{
		if (Def.StageId.IsNone())
		{
			OutErrors.Add(TEXT("StageId 가 비어 있는 스테이지 보상이 있습니다."));
			continue;
		}

		const FName RewardId = MakeStageClearRewardId(Def.StageId);
		if (SeenIds.Contains(RewardId))
		{
			OutErrors.Add(FString::Printf(TEXT("중복 StageId: %s"), *Def.StageId.ToString()));
		}
		SeenIds.Add(RewardId);
	}

	for (const FDRAchievementDef& Def : Achievements)
	{
		if (Def.AchievementId.IsNone())
		{
			OutErrors.Add(TEXT("AchievementId 가 비어 있는 업적이 있습니다."));
			continue;
		}

		if (SeenIds.Contains(Def.AchievementId))
		{
			OutErrors.Add(FString::Printf(TEXT("보상 Id 충돌: %s"), *Def.AchievementId.ToString()));
		}
		SeenIds.Add(Def.AchievementId);
	}

	// 업그레이드 시스템 해금 스테이지가 정확히 1개인지
	int32 UnlockStageCount = 0;
	for (const FDRStageRewardDef& Def : StageRewards)
	{
		if (Def.bUnlocksUpgradeSystem)
		{
			++UnlockStageCount;
		}
	}
	if (UnlockStageCount == 0)
	{
		OutErrors.Add(TEXT("bUnlocksUpgradeSystem = true 인 스테이지가 없어 업그레이드 시스템이 영구히 잠깁니다."));
	}

	if (!ChipCatalog)
	{
		OutErrors.Add(TEXT("ChipCatalog 이 지정되지 않았습니다."));
	}
	else
	{
		TArray<FString> CatalogErrors;
		ChipCatalog->ValidateCatalog(GetMaxSlotCount(), CatalogErrors);
		OutErrors.Append(CatalogErrors);

		// ===== 기본 장착 칩 검증 (Plan3(공용칩) §6.4) =====
		// ★칩 정의(카탈로그)와 시작 해금 수(Config)를 ★둘 다★ 알아야 하므로 여기서만 검사할 수 있다.
		// RequiredSlotTier 가 시작 해금 수보다 크면 SanitizeClassState() 가 다음 실행에서
		// 그 칩을 조용히 빼 버린다 — 증상이 늦게 나타나 디버깅이 어렵다.
		const int32 FreeSlots = FMath::Clamp(InitialFreeSlots, 0, GetMaxSlotCount());

		if (DefaultEquippedChips.Num() > FreeSlots)
		{
			OutErrors.Add(FString::Printf(
				TEXT("DefaultEquippedChips 가 %d개인데 InitialFreeSlots 가 %d 라서 일부는 장착되지 않습니다."),
				DefaultEquippedChips.Num(), FreeSlots));
		}

		TSet<FName> SeenDefaults;
		for (const FName& DefaultId : DefaultEquippedChips)
		{
			if (DefaultId.IsNone())
			{
				OutErrors.Add(TEXT("DefaultEquippedChips 에 비어 있는 항목이 있습니다."));
				continue;
			}

			if (SeenDefaults.Contains(DefaultId))
			{
				OutErrors.Add(FString::Printf(
					TEXT("DefaultEquippedChips 에 중복된 칩이 있습니다: %s"), *DefaultId.ToString()));
			}
			SeenDefaults.Add(DefaultId);

			const FDRUpgradeChipDefinition* Chip = ChipCatalog->FindChip(DefaultId);
			if (!Chip)
			{
				OutErrors.Add(FString::Printf(
					TEXT("기본 장착 칩 %s 가 카탈로그에 없습니다."), *DefaultId.ToString()));
				continue;
			}

			if (Chip->RequiredSlotTier > FreeSlots)
			{
				OutErrors.Add(FString::Printf(
					TEXT("기본 장착 칩 %s 의 RequiredSlotTier(%d) 가 InitialFreeSlots(%d) 보다 커서 ")
					TEXT("세이브 정화 시 자동으로 해제됩니다."),
					*DefaultId.ToString(), Chip->RequiredSlotTier, FreeSlots));
			}

			// 로봇 전용 칩을 기본 장착으로 지정하면 나머지 로봇에서는 조용히 빠진다
			if (!Chip->bCommonChip)
			{
				OutErrors.Add(FString::Printf(
					TEXT("기본 장착 칩 %s 가 공용 칩이 아닙니다 — %s 이외의 로봇에서는 장착되지 않습니다."),
					*DefaultId.ToString(),
					*UEnum::GetDisplayValueAsText(Chip->OwnerClass).ToString()));
			}
		}
	}

	// ===== ★Plan5(잭팟칩)★ 잭팟 확률표 검사 (D-61) =====
	// 합이 1 이 아니어도 코드는 정규화로 버티지만, 그러면 디자이너가 쓴 숫자와 다른 확률이 조용히 나간다.
	{
		const FDRJackpotChipTuning& Jackpot = ChipTuning.Jackpot;

		auto CheckOdds = [&OutErrors](const TCHAR* Name, const FDRJackpotOdds& Odds)
		{
			if (Odds.Bronze < 0.f || Odds.Silver < 0.f || Odds.Gold < 0.f)
			{
				OutErrors.Add(FString::Printf(TEXT("ChipTuning.Jackpot.%s 에 음수 확률이 있습니다."), Name));
			}
			if (!FMath::IsNearlyEqual(Odds.Sum(), 1.f, 0.001f))
			{
				OutErrors.Add(FString::Printf(
					TEXT("ChipTuning.Jackpot.%s 의 합이 %.3f 입니다(브론즈 %.3f + 실버 %.3f + 골드 %.3f) — 1.0 이 되게 맞추세요."),
					Name, Odds.Sum(), Odds.Bronze, Odds.Silver, Odds.Gold));
			}
		};

		CheckOdds(TEXT("GoldRushOdds"), Jackpot.GoldRushOdds);
		CheckOdds(TEXT("PityBaseOdds"), Jackpot.PityBaseOdds);

		if (Jackpot.PityStep <= 0.f)
		{
			OutErrors.Add(TEXT("ChipTuning.Jackpot.PityStep 이 0 이하라 적립 카드의 보정이 쌓이지 않습니다."));
		}
		if (Jackpot.DoubleShotDelay <= 0.f)
		{
			OutErrors.Add(TEXT("ChipTuning.Jackpot.DoubleShotDelay 가 0 이하입니다."));
		}
	}

	// ===== ★Plan5(공속칩)★ 공속 버프 칩 GE 검사 =====
	// GE 를 비우거나 설정을 틀리면 칩이 ★에러 없이★ 기본 버프처럼 동작한다(경고 로그는 시전할 때만 찍힌다).
	// 칩이 카탈로그에 있을 때만 검사한다 — 코드가 에셋보다 먼저 들어가는 기간에 오류가 쏟아지지 않게.
	{
		const FDRGameplayTags& Tags = FDRGameplayTags::Get();
		const FDRAttackSpeedBuffChipTuning& Speed = ChipTuning.AttackSpeedBuff;

		auto CheckBuffEffect = [&OutErrors](const TCHAR* FieldName, const TSoftClassPtr<UGameplayEffect>& Effect,
			const FGameplayTag& RequiredTag, bool bRequireDecayPolicy)
		{
			if (Effect.IsNull())
			{
				OutErrors.Add(FString::Printf(
					TEXT("ChipTuning.AttackSpeedBuff.%s 가 비어 있어 칩이 기본 공속 버프로 동작합니다."), FieldName));
				return;
			}

			const UClass* EffectClass = Effect.LoadSynchronous();
			const UGameplayEffect* CDO = EffectClass ? EffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
			if (!CDO) return;

			if (!CDO->GetGrantedTags().HasTagExact(RequiredTag))
			{
				OutErrors.Add(FString::Printf(
					TEXT("ChipTuning.AttackSpeedBuff.%s(%s)의 Target Tags 에 %s 가 없습니다 — 공속 계산이 이 버프를 못 읽습니다."),
					FieldName, *CDO->GetName(), *RequiredTag.ToString()));
			}
			if (CDO->GetStackLimitCount() <= 1)
			{
				OutErrors.Add(FString::Printf(
					TEXT("ChipTuning.AttackSpeedBuff.%s(%s)의 Stack Limit Count 가 1 이하라 중첩이 쌓이지 않습니다."),
					FieldName, *CDO->GetName()));
			}
			if (bRequireDecayPolicy
				&& CDO->GetStackExpirationPolicy() != EGameplayEffectStackingExpirationPolicy::RemoveSingleStackAndRefreshDuration)
			{
				OutErrors.Add(FString::Printf(
					TEXT("ChipTuning.AttackSpeedBuff.%s(%s)의 Stack Expiration Policy 가 'Remove Single Stack and Refresh Duration' 이 아니라 ")
					TEXT("중첩이 1개씩 줄지 않습니다 (Plan5 D-72)."),
					FieldName, *CDO->GetName()));
			}
		};

		if (ChipCatalog && ChipCatalog->IsFeatureGated(Tags.Upgrade_Feature_Skill_AttackSpeedBuff_Share))
		{
			CheckBuffEffect(TEXT("SharedEffect"), Speed.SharedEffect, Tags.Buff_AttackSpeed_Shared, false);
		}
		if (ChipCatalog && ChipCatalog->IsFeatureGated(Tags.Upgrade_Feature_Skill_AttackSpeedBuff_Decay))
		{
			CheckBuffEffect(TEXT("DecayEffect"), Speed.DecayEffect, Tags.Buff_VendingMachine_AttackSpeed, true);
		}
	}

	if (OutErrors.Num() > 0)
	{
		UE_LOG(LogDR, Warning, TEXT("[ProgressionConfig] %s: 검증 오류 %d건 (첫 항목: %s)"),
			*GetName(), OutErrors.Num(), *OutErrors[0]);
	}
	else
	{
		UE_LOG(LogDR, Log, TEXT("[ProgressionConfig] %s: 검증 통과. 재화 예산 %d / 필요 %d"),
			*GetName(), Budget, Required);
	}

	return OutErrors.Num() == 0;
}

#if WITH_EDITOR
void UDRProgressionConfig::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 편집 즉시 검증 — 재화 예산 미달/칩 정합성 오류를 로그로 알린다.
	TArray<FString> Errors;
	ValidateCurrencyBudget(Errors);

	for (const FString& Error : Errors)
	{
		UE_LOG(LogDR, Warning, TEXT("[ProgressionConfig] %s"), *Error);
	}
}
#endif
