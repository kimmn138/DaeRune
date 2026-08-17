// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/DRUpgradeTypes.h"
#include "DRProgressionConfig.generated.h"

class UDRChipCatalog;

/**
 * 스테이지 최초 클리어 보상 정의. 계정당 1회만 지급된다.
 * 원장 키는 StageId 로부터 결정적으로 생성한다(UDRProgressionConfig::MakeStageClearRewardId).
 * (Plan2.md 5.1 참조)
 */
USTRUCT(BlueprintType)
struct FDRStageRewardDef
{
	GENERATED_BODY()

	// ADRStageGameMode::StageId 와 일치해야 한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage")
	FName StageId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage")
	FText DisplayName;

	// 최초 클리어 지급액
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage", meta = (ClampMin = "0"))
	int32 FirstClearCurrency = 1200;

	// 이 스테이지를 최초 클리어하면 업그레이드 시스템이 해금된다 (스테이지1 = true)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage")
	bool bUnlocksUpgradeSystem = false;
};

/**
 * 업적 / 클리어 조건 보상 정의. 달성 판정은 서버(페이즈/게임모드)가 하고,
 * 지급과 중복 방지는 클라이언트 세이브 원장이 담당한다. 전부 계정당 1회다.
 * (Plan2.md 5.2 참조)
 */
USTRUCT(BlueprintType)
struct FDRAchievementDef
{
	GENERATED_BODY()

	// 서버가 보고하는 키. 변경 금지.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Achievement")
	FName AchievementId;

	// 결과창 라벨/업적 화면 분류용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Achievement")
	EDRRewardKind Kind = EDRRewardKind::Achievement;

	// 소속 스테이지 (업적 목록 그룹핑용, 선택)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Achievement")
	FName StageId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Achievement")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Achievement")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Achievement", meta = (ClampMin = "0"))
	int32 Currency = 400;
};

/**
 * 재화 획득 규칙 + 슬롯 해금 비용 + 칩 카탈로그 참조를 담는 DataAsset. 디자이너 튜닝용.
 * 서버/클라 공통 접근을 위해 UDRGameInstance 가 보유한다. (Plan2.md 4.5 참조)
 */
UCLASS(BlueprintType)
class DAERUNE_API UDRProgressionConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// ========== 칩 정의 ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TObjectPtr<UDRChipCatalog> ChipCatalog;

	// ========== 슬롯 ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot", meta = (ClampMin = "0"))
	int32 MaxStatSlots = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot", meta = (ClampMin = "0"))
	int32 MaxAscensionSlots = 3;

	// 스탯 슬롯 해금 비용 (index = 해금할 슬롯 번호-1). 비면 DefaultSlotCost * 번호.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot")
	TArray<int32> StatSlotCosts;

	// 돌파 슬롯 해금 비용 (index = 해금할 슬롯 번호-1). 비면 DefaultSlotCost * 번호.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot")
	TArray<int32> AscensionSlotCosts;

	// 비용 배열이 비었을 때의 폴백 단가
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot", meta = (ClampMin = "0"))
	int32 DefaultSlotCost = 500;

	// 마지막으로 해금한 슬롯의 환불 허용 여부 (다른 로봇으로 방향을 틀 수 있게 하는 안전망)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot")
	bool bAllowSlotRefund = true;

	// 슬롯 환불 시 돌려받는 비율 (1.0 = 전액)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RefundRatio = 1.0f;

	// ========== 재화 획득 (전부 계정당 1회) ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reward", meta = (TitleProperty = "StageId"))
	TArray<FDRStageRewardDef> StageRewards;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reward", meta = (TitleProperty = "AchievementId"))
	TArray<FDRAchievementDef> Achievements;

	// ========== 조회 헬퍼 ==========

	// 카테고리별 최대 슬롯 수
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetMaxSlotCount(EDRChipCategory Category) const;

	// SlotNumber(1-base) 슬롯을 해금하는 비용. 범위를 벗어나면 -1.
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetSlotUnlockCost(EDRChipCategory Category, int32 SlotNumber) const;

	// 스테이지 최초 클리어 보상 정의. 없으면 nullptr.
	const FDRStageRewardDef* FindStageReward(FName StageId) const;

	// 업적 정의. 없으면 nullptr.
	const FDRAchievementDef* FindAchievement(FName AchievementId) const;

	// 스테이지 최초 클리어 원장 키 ("StageClear.<StageId>")
	UFUNCTION(BlueprintPure, Category = "Progression")
	static FName MakeStageClearRewardId(FName StageId);

	// ========== 예산 검증 (요구사항: 최종적으로 모든 슬롯을 해금할 수 있어야 한다) ==========

	// 게임 전체에서 획득 가능한 재화 총량
	UFUNCTION(BlueprintPure, Category = "Progression|Budget")
	int32 GetTotalCurrencyBudget() const;

	// 모든 로봇의 모든 슬롯을 해금하는 데 필요한 재화 총량
	UFUNCTION(BlueprintPure, Category = "Progression|Budget")
	int32 GetTotalSlotUnlockCost() const;

	// 한 로봇의 모든 슬롯 해금 비용
	UFUNCTION(BlueprintPure, Category = "Progression|Budget")
	int32 GetSlotUnlockCostForClass() const;

	// 총 획득 >= 총 필요 인지 검사. 콘텐츠 작업 후 1회 실행할 것. (Plan2.md 5.4)
	UFUNCTION(BlueprintCallable, Category = "Progression|Budget")
	bool ValidateCurrencyBudget(TArray<FString>& OutErrors) const;

#if WITH_EDITOR
	// 에셋을 편집할 때마다 예산/카탈로그 정합성을 자동 검사해 로그로 알린다.
	// (수동 호출을 잊어 재화가 모자란 채 빌드되는 것을 막기 위함)
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
