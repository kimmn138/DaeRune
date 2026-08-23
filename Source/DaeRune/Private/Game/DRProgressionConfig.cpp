// Copyright DaeRune


#include "Game/DRProgressionConfig.h"
#include "Game/DRChipCatalog.h"
#include "DaeRune/DRLogChannels.h"

int32 UDRProgressionConfig::GetMaxSlotCount(EDRChipCategory Category) const
{
	return FMath::Max(0, Category == EDRChipCategory::Ascension ? MaxAscensionSlots : MaxStatSlots);
}

int32 UDRProgressionConfig::GetSlotUnlockCost(EDRChipCategory Category, int32 SlotNumber) const
{
	if (SlotNumber <= 0 || SlotNumber > GetMaxSlotCount(Category))
	{
		return -1;
	}

	const TArray<int32>& Costs = (Category == EDRChipCategory::Ascension) ? AscensionSlotCosts : StatSlotCosts;
	if (Costs.IsValidIndex(SlotNumber - 1))
	{
		return FMath::Max(0, Costs[SlotNumber - 1]);
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

	const int32 CategoryCount = static_cast<int32>(EDRChipCategory::Count);
	for (int32 CategoryIndex = 0; CategoryIndex < CategoryCount; ++CategoryIndex)
	{
		const EDRChipCategory Category = static_cast<EDRChipCategory>(CategoryIndex);
		const int32 MaxSlots = GetMaxSlotCount(Category);
		for (int32 SlotNumber = 1; SlotNumber <= MaxSlots; ++SlotNumber)
		{
			Total += FMath::Max(0, GetSlotUnlockCost(Category, SlotNumber));
		}
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
		ChipCatalog->ValidateCatalog(MaxStatSlots, MaxAscensionSlots, CatalogErrors);
		OutErrors.Append(CatalogErrors);
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
