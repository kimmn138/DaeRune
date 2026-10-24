// Copyright DaeRune


#include "Game/DRProgressionConfig.h"
#include "Game/DRChipCatalog.h"
#include "Game/DRCosmeticCatalog.h"
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

bool UDRProgressionConfig::ValidateAchievements(TArray<FString>& OutErrors) const
{
	OutErrors.Reset();

	// 스팀 API 이름 — 영문 · 숫자 · '_' 만 (파트너 사이트 규칙), 중복 금지
	TMap<FString, FName> SteamNameOwners;
	for (const FDRAchievementDef& Def : Achievements)
	{
		if (Def.SteamApiName.IsEmpty()) continue;	// 로컬 전용 업적 (Plan(옷장) D-20)

		for (const TCHAR Ch : Def.SteamApiName)
		{
			const bool bAllowed = (Ch >= TEXT('A') && Ch <= TEXT('Z')) || (Ch >= TEXT('a') && Ch <= TEXT('z'))
				|| (Ch >= TEXT('0') && Ch <= TEXT('9')) || Ch == TEXT('_');
			if (!bAllowed)
			{
				OutErrors.Add(FString::Printf(TEXT("[%s] SteamApiName '%s' 에 쓸 수 없는 문자가 있습니다 — 영문 · 숫자 · '_' 만"),
					*Def.AchievementId.ToString(), *Def.SteamApiName));
				break;
			}
		}

		if (const FName* Owner = SteamNameOwners.Find(Def.SteamApiName))
		{
			OutErrors.Add(FString::Printf(TEXT("SteamApiName '%s' 가 %s 와 %s 에 겹칩니다 — 한쪽이 조용히 덮입니다"),
				*Def.SteamApiName, *Owner->ToString(), *Def.AchievementId.ToString()));
		}
		else
		{
			SteamNameOwners.Add(Def.SteamApiName, Def.AchievementId);
		}
	}

	// 옷 해금 조건이 실재하는가 — IsSkinUnlocked 는 EarnedAchievements ∪ ClaimedRewards 를 보므로
	// 유효한 키는 "업적 Id" 와 "StageClear.<StageId>" 두 종류뿐이다
	if (CosmeticCatalog)
	{
		TSet<FName> ValidIds;
		for (const FDRAchievementDef& Def : Achievements)
		{
			if (!Def.AchievementId.IsNone()) ValidIds.Add(Def.AchievementId);
		}
		for (const FDRStageRewardDef& Def : StageRewards)
		{
			if (!Def.StageId.IsNone()) ValidIds.Add(MakeStageClearRewardId(Def.StageId));
		}

		for (const FDRSkinDefinition& Skin : CosmeticCatalog->Skins)
		{
			for (const FName& Required : Skin.RequiredAchievements)
			{
				if (!ValidIds.Contains(Required))
				{
					OutErrors.Add(FString::Printf(
						TEXT("[%s] 해금 조건 '%s' 가 업적에도 스테이지 클리어(StageClear.<StageId>)에도 없습니다 — 영원히 잠깁니다"),
						*Skin.SkinId.ToString(), *Required.ToString()));
				}
			}
		}
	}

	return OutErrors.Num() == 0;
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

	// ===== ★Plan5(탑승칩 · 공기탄칩 · 물점프칩)★ 청소기 칩 GE 검사 =====
	// 공속칩과 같은 이유 — 비우거나 설정을 틀리면 칩이 에러 없이 일부만 동작한다. 칩이 카탈로그에 있을 때만 검사한다.
	{
		const FDRGameplayTags& Tags = FDRGameplayTags::Get();

		// 필드가 비었으면 오류를 남기고 nullptr, 아니면 GE CDO
		auto LoadEffectCDO = [&OutErrors](const TCHAR* FieldName, const TSoftClassPtr<UGameplayEffect>& Effect,
			const TCHAR* WhatBreaks) -> const UGameplayEffect*
		{
			if (Effect.IsNull())
			{
				OutErrors.Add(FString::Printf(TEXT("ChipTuning.%s 가 비어 있어 %s"), FieldName, WhatBreaks));
				return nullptr;
			}
			const UClass* EffectClass = Effect.LoadSynchronous();
			return EffectClass ? EffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
		};

		auto RequireGrantedTag = [&OutErrors](const TCHAR* FieldName, const UGameplayEffect* CDO, const FGameplayTag& Tag,
			const TCHAR* WhatBreaks)
		{
			if (CDO && !CDO->GetGrantedTags().HasTagExact(Tag))
			{
				OutErrors.Add(FString::Printf(TEXT("ChipTuning.%s(%s)의 Target Tags 에 %s 가 없습니다 — %s"),
					FieldName, *CDO->GetName(), *Tag.ToString(), WhatBreaks));
			}
		};

		// 이 속성에 이 연산의 모디파이어가 있나
		auto HasModifier = [](const UGameplayEffect* CDO, const FName& AttributeName, EGameplayModOp::Type Op) -> bool
		{
			for (const FGameplayModifierInfo& Mod : CDO->Modifiers)
			{
				if (Mod.Attribute.GetName() == AttributeName.ToString() && Mod.ModifierOp == Op) return true;
			}
			return false;
		};

		// ① 탑승 감속 (Plan5 D-80 · T-84 · T-86)
		if (ChipCatalog && ChipCatalog->IsFeatureGated(Tags.Upgrade_Feature_Skill_Ride_Power))
		{
			if (const UGameplayEffect* CDO = LoadEffectCDO(TEXT("Ride.PowerLoadEffect"), ChipTuning.Ride.PowerLoadEffect,
					TEXT("탑승칩①이 감속 없이 공격력만 올립니다.")))
			{
				RequireGrantedTag(TEXT("Ride.PowerLoadEffect"), CDO, Tags.Debuff_RobotVacuum_RideLoad,
					TEXT("코드가 감속을 못 지워 갱신마다 쌓입니다 (Plan5 T-84)."));
				if (CDO->DurationPolicy != EGameplayEffectDurationType::Infinite)
				{
					OutErrors.Add(FString::Printf(TEXT("ChipTuning.Ride.PowerLoadEffect(%s)가 Infinite 가 아니라 감속이 저절로 사라집니다."), *CDO->GetName()));
				}
				if (!HasModifier(CDO, TEXT("MoveSpeed"), EGameplayModOp::MultiplyCompound))
				{
					OutErrors.Add(FString::Printf(TEXT("ChipTuning.Ride.PowerLoadEffect(%s)에 MoveSpeed · Multiply (Compound) 모디파이어가 없습니다 (Plan5 T-86)."), *CDO->GetName()));
				}
			}
		}

		// ③ 비상 탈출 회복 · 금지 (Plan5 D-89 · D-90 · T-81)
		if (ChipCatalog && ChipCatalog->IsFeatureGated(Tags.Upgrade_Feature_Skill_Ride_Eject))
		{
			LoadEffectCDO(TEXT("Ride.EjectHealEffect"), ChipTuning.Ride.EjectHealEffect, TEXT("탑승칩③이 회복 없이 탈출 · 금지만 합니다."));
			if (const UGameplayEffect* CDO = LoadEffectCDO(TEXT("Ride.EjectBanEffect"), ChipTuning.Ride.EjectBanEffect,
					TEXT("탑승칩③이 탑승 금지 없이 탈출 · 회복만 합니다.")))
			{
				RequireGrantedTag(TEXT("Ride.EjectBanEffect"), CDO, Tags.Debuff_Ride_Banned,
					TEXT("탑승 금지가 판정되지 않습니다."));
				if (CDO->StackingType == EGameplayEffectStackingType::AggregateByTarget)
				{
					OutErrors.Add(FString::Printf(TEXT("ChipTuning.Ride.EjectBanEffect(%s)가 Aggregate by Target 이라 두 번째 청소기의 금지가 첫 GE 에 합쳐집니다 — Stacking None (Plan5 T-81)."), *CDO->GetName()));
				}
			}
			if (ChipTuning.Ride.EjectBanDuration <= 0.f)
			{
				OutErrors.Add(TEXT("ChipTuning.Ride.EjectBanDuration 이 0 이하입니다."));
			}
		}

		// 공기탄② 흡착 (Plan5 D-96 · T-95 · T-96 · T-97)
		if (ChipCatalog && ChipCatalog->IsFeatureGated(Tags.Upgrade_Feature_Skill_AirShot_Drag))
		{
			const FDRAirShotChipTuning& AirShot = ChipTuning.AirShot;
			if (const UGameplayEffect* CDO = LoadEffectCDO(TEXT("AirShot.Drag.Effect"), AirShot.Drag.Effect,
					TEXT("공기탄칩②가 감속 · 받는 피해 증가 없이 피해 −20% 만 남습니다.")))
			{
				RequireGrantedTag(TEXT("AirShot.Drag.Effect"), CDO, Tags.Debuff_AirShot_Drag,
					TEXT("받는 피해 증가가 읽히지 않습니다."));
				if (CDO->StackingType != EGameplayEffectStackingType::AggregateByTarget)
				{
					OutErrors.Add(FString::Printf(TEXT("ChipTuning.AirShot.Drag.Effect(%s)가 Aggregate by Target 이 아니라 청소기마다 따로 쌓입니다."), *CDO->GetName()));
				}
				if (CDO->GetStackLimitCount() != AirShot.DragMaxStacks)
				{
					OutErrors.Add(FString::Printf(TEXT("ChipTuning.AirShot.Drag.Effect(%s)의 Stack Limit(%d)이 DragMaxStacks(%d)와 달라 감속 스택과 피해 증가 스택이 어긋납니다."),
						*CDO->GetName(), CDO->GetStackLimitCount(), AirShot.DragMaxStacks));
				}
				if (CDO->GetStackExpirationPolicy() != EGameplayEffectStackingExpirationPolicy::ClearEntireStack)
				{
					OutErrors.Add(FString::Printf(TEXT("ChipTuning.AirShot.Drag.Effect(%s)의 Stack Expiration Policy 가 Clear Entire Stack 이 아닙니다 (Plan5 T-97)."), *CDO->GetName()));
				}
				if (CDO->bDenyOverflowApplication)
				{
					OutErrors.Add(FString::Printf(TEXT("ChipTuning.AirShot.Drag.Effect(%s)의 Deny Overflow Application 이 켜져 있어 3스택에서 지속이 갱신되지 않습니다 (Plan5 T-96)."), *CDO->GetName()));
				}
				if (!HasModifier(CDO, TEXT("MoveSpeed"), EGameplayModOp::MultiplyAdditive))
				{
					OutErrors.Add(FString::Printf(TEXT("ChipTuning.AirShot.Drag.Effect(%s)에 MoveSpeed · ★Multiply (Additive)★ 모디파이어가 없습니다 — Compound 면 3스택에서 오히려 빨라집니다 (Plan5 T-95)."), *CDO->GetName()));
				}
			}
			if (AirShot.Drag.Magnitude <= 0.f || AirShot.Drag.Magnitude >= 1.f || AirShot.Drag.Duration <= 0.f)
			{
				OutErrors.Add(TEXT("ChipTuning.AirShot.Drag 의 Magnitude 는 0~1 사이, Duration 은 0 보다 커야 합니다."));
			}
		}

		// 물 점프② 무적 (Plan5 D-108 · T-113)
		if (ChipCatalog && ChipCatalog->IsFeatureGated(Tags.Upgrade_Feature_Skill_JetJump_Guard))
		{
			if (const UGameplayEffect* CDO = LoadEffectCDO(TEXT("JetJump.GuardEffect"), ChipTuning.JetJump.GuardEffect,
					TEXT("물점프칩②가 무적 없이 피해 0 만 남습니다.")))
			{
				RequireGrantedTag(TEXT("JetJump.GuardEffect"), CDO, Tags.State_Invulnerable,
					TEXT("무적이 판정되지 않습니다."));
				if (CDO->DurationPolicy != EGameplayEffectDurationType::HasDuration
					|| CDO->DurationMagnitude.GetMagnitudeCalculationType() != EGameplayEffectMagnitudeCalculation::SetByCaller)
				{
					OutErrors.Add(FString::Printf(TEXT("ChipTuning.JetJump.GuardEffect(%s)는 Has Duration · Duration = Set by Caller(Data.Duration) 이어야 합니다 (Plan5 T-113)."), *CDO->GetName()));
				}
			}
			if (ChipTuning.JetJump.GuardDuration <= 0.f)
			{
				OutErrors.Add(TEXT("ChipTuning.JetJump.GuardDuration 이 0 이하입니다."));
			}
		}
	}

	// ===== ★Plan5(가방칩 · 도발칩 · 모자칩)★ 벨보이 칩 튜닝 검사 =====
	// 비우거나 0 으로 두면 칩이 에러 없이 일부만 동작한다(가방②는 물만 쓰고, 모자①은 단점이 사라진다).
	// 청소기 칩과 같은 이유로 그 칩이 카탈로그에 있을 때만 검사한다.
	{
		const FDRGameplayTags& Tags = FDRGameplayTags::Get();

		if (ChipCatalog && ChipCatalog->IsFeatureGated(Tags.Upgrade_Feature_Skill_Bag_HatDrop))
		{
			if (ChipTuning.Bag.HatPickupClass.IsNull())
			{
				OutErrors.Add(TEXT("ChipTuning.Bag.HatPickupClass 가 비어 있어 가방칩②가 모자 없이 물만 씁니다."));
			}
			if (ChipTuning.Bag.HatDropChance <= 0.f)
			{
				OutErrors.Add(TEXT("ChipTuning.Bag.HatDropChance 가 0 이하라 가방칩②의 모자가 떨어지지 않습니다."));
			}
		}

		if (ChipCatalog && ChipCatalog->IsFeatureGated(Tags.Upgrade_Feature_Skill_Taunt_Expose)
			&& ChipTuning.Taunt.ExposeDamageTakenBonus <= 0.f)
		{
			OutErrors.Add(TEXT("ChipTuning.Taunt.ExposeDamageTakenBonus 가 0 이하라 도발칩①이 단점(공격력 감소 해제)만 남습니다."));
		}

		if (ChipCatalog && ChipCatalog->IsFeatureGated(Tags.Upgrade_Feature_Skill_Hat_AllyHit)
			&& ChipTuning.Hat.AllyHitDamage <= 0.f)
		{
			OutErrors.Add(TEXT("ChipTuning.Hat.AllyHitDamage 가 0 이하라 모자칩①의 단점(아군 피해)이 사라집니다."));
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

	// 업적 · 스팀 키 · 옷 해금 조건 (Plan(옷장) D-29)
	TArray<FString> AchievementErrors;
	ValidateAchievements(AchievementErrors);
	Errors.Append(AchievementErrors);

	for (const FString& Error : Errors)
	{
		UE_LOG(LogDR, Warning, TEXT("[ProgressionConfig] %s"), *Error);
	}
}
#endif
