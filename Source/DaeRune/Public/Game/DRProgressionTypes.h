// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRUpgradeTypes.h"
#include "DRProgressionTypes.generated.h"

/**
 * 한 로봇(클래스)의 업그레이드 슬롯/장착 상태. (세이브 저장 단위)
 *
 * 슬롯은 카테고리별로 순차 해금한다(UnlockedStatSlots / UnlockedAscensionSlots).
 * 칸 배열(StatSlotChips / AscensionSlotChips)은 UI 의 01~06 칸과 1:1 대응하며,
 * 여러 칸을 점유하는 칩(RequiredSlotCount >= 2)은 같은 ChipId 가 여러 칸에 기록된다.
 * → 점유 칸 수 = 그 Id 의 등장 횟수. 자료구조만으로 정합성이 유지된다.
 * (Plan2.md 4.2 참조)
 */
USTRUCT(BlueprintType)
struct FDRClassUpgradeState
{
	GENERATED_BODY()

	// 해금한 스탯 슬롯 수 (0 .. MaxStatSlots)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrade")
	int32 UnlockedStatSlots = 0;

	// 해금한 돌파 슬롯 수 (0 .. MaxAscensionSlots)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrade")
	int32 UnlockedAscensionSlots = 0;

	// 스탯 슬롯 칸별 점유 칩 (없으면 NAME_None)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<FName> StatSlotChips;

	// 돌파 슬롯 칸별 점유 칩 (없으면 NAME_None)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<FName> AscensionSlotChips;

	// 슬롯 해금에 지불한 총액 (환불 상한)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrade")
	int32 SpentCurrency = 0;

	// ===== 카테고리 기반 접근 (호출부에 if (Category == Stat) 분기를 복제하지 않기 위한 헬퍼) =====

	const TArray<FName>& GetSlotChips(EDRChipCategory Category) const
	{
		return Category == EDRChipCategory::Ascension ? AscensionSlotChips : StatSlotChips;
	}

	TArray<FName>& GetSlotChips(EDRChipCategory Category)
	{
		return Category == EDRChipCategory::Ascension ? AscensionSlotChips : StatSlotChips;
	}

	int32 GetUnlockedSlotCount(EDRChipCategory Category) const
	{
		return Category == EDRChipCategory::Ascension ? UnlockedAscensionSlots : UnlockedStatSlots;
	}

	void SetUnlockedSlotCount(EDRChipCategory Category, int32 NewCount)
	{
		if (Category == EDRChipCategory::Ascension)
		{
			UnlockedAscensionSlots = NewCount;
		}
		else
		{
			UnlockedStatSlots = NewCount;
		}
	}

	// 해금된 칸 중 비어 있는 칸 수
	int32 CountFreeSlots(EDRChipCategory Category) const
	{
		const TArray<FName>& Slots = GetSlotChips(Category);
		const int32 Unlocked = FMath::Min(GetUnlockedSlotCount(Category), Slots.Num());

		int32 Free = 0;
		for (int32 Index = 0; Index < Unlocked; ++Index)
		{
			if (Slots[Index].IsNone())
			{
				++Free;
			}
		}
		return Free;
	}

	// 그 칩이 점유한 칸 수 (0 = 미장착)
	int32 CountOccupiedSlots(EDRChipCategory Category, FName ChipId) const
	{
		if (ChipId.IsNone()) return 0;

		int32 Count = 0;
		for (const FName& Slot : GetSlotChips(Category))
		{
			if (Slot == ChipId)
			{
				++Count;
			}
		}
		return Count;
	}

	// 카테고리 구분 없이 장착된 모든 칩 Id (중복 없이). 서버 보고/런타임 해석용.
	void GetEquippedChips(TArray<FName>& OutChips) const
	{
		OutChips.Reset();
		for (const FName& Slot : StatSlotChips)
		{
			if (!Slot.IsNone())
			{
				OutChips.AddUnique(Slot);
			}
		}
		for (const FName& Slot : AscensionSlotChips)
		{
			if (!Slot.IsNone())
			{
				OutChips.AddUnique(Slot);
			}
		}
	}
};

/**
 * 서버 → 클라이언트로 전달되는 "이번 스테이지 성과" 원본 데이터.
 * 재화 환산과 1회성 판정은 클라이언트가 로컬 세이브 원장으로 수행한다.
 * (Plan2.md 5.1 / 7.3 참조)
 */
USTRUCT(BlueprintType)
struct FDRStageRewardReport
{
	GENERATED_BODY()

	// 어떤 스테이지인가 (ADRStageGameMode::StageId)
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	FName StageId;

	// 이번 스테이지에서 실제로 플레이한 로봇 (결과창 표기/통계용)
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	EPlayerCharacterClass PlayedClass = EPlayerCharacterClass::Gardener;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	bool bGameClear = false;

	// 완료한 페이즈 수 (표시용 — 재화로 환산하지 않는다)
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	int32 ClearedPhaseCount = 0;

	// 이 플레이어가 이번 스테이지에서 처치한 적 수 (표시용)
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	int32 KillCount = 0;

	// 이번 스테이지에서 달성한 업적/클리어 조건 Id (서버 판정)
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	TArray<FName> AchievementIds;
};

/** 결과창에 한 줄로 표시할 재화 획득 내역. */
USTRUCT(BlueprintType)
struct FDRRewardLineItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	FName RewardId;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	EDRRewardKind Kind = EDRRewardKind::Achievement;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	int32 Currency = 0;
};

/**
 * 보상 반영 결과 요약. 결과창 연출용.
 * 이미 받은 보상은 Lines 에 담지 않는다(중복 표기 방지). (Plan2.md 10.3 참조)
 */
USTRUCT(BlueprintType)
struct FDRStageRewardResult
{
	GENERATED_BODY()

	// 스테이지 최초 클리어 보상
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	int32 FirstClearCurrency = 0;

	// 업적 + 클리어 조건 보상 합
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	int32 AchievementCurrency = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	int32 TotalGained = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	int32 CurrencyBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	int32 CurrencyAfter = 0;

	// 이번에 업그레이드 시스템이 해금됐는가 (해금 연출용)
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	bool bUpgradeSystemNewlyUnlocked = false;

	// 항목별 내역 (결과창 롤업 연출에 그대로 사용)
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	TArray<FDRRewardLineItem> Lines;
};

/**
 * 업그레이드 화면의 칩 카드 1개를 그리는 데 필요한 모든 정보.
 * UI 가 카탈로그와 세이브를 직접 뒤지지 않게 하기 위한 뷰모델. (Plan2.md 10.1 참조)
 */
USTRUCT(BlueprintType)
struct FDRChipViewModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	FName ChipId;

	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	EDRChipCategory Category = EDRChipCategory::Stat;

	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	FText DisplayName;

	// 적용 대상 스킬 표시명 (태그가 없으면 "전체")
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	FText TargetSkillName;

	// 이해하기 쉬운 효과 요약 (기본 표시)
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	FText EffectSummary;

	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	TObjectPtr<UTexture2D> Icon;

	// 필요한 슬롯 수
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	int32 RequiredSlotCount = 1;

	// 해금에 필요한 슬롯 해금 단계
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	int32 RequiredSlotTier = 1;

	// 장착 여부
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	bool bEquipped = false;

	// 해금 여부 (RequiredSlotTier 충족)
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	bool bUnlocked = false;

	// 지금 장착할 수 있는가 (빈 슬롯 수까지 고려)
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	bool bCanEquipNow = false;

	// 장착 불가 사유 (bCanEquipNow == false 일 때 UI 툴팁/경고에 사용)
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	EDRUpgradeResult EquipBlockReason = EDRUpgradeResult::Success;

	// 단점 모디파이어 보유 여부 (경고 아이콘)
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	bool bHasDrawback = false;

	// 세부 정보 토글 ON 일 때 표시할 상세 수치 (장점 + 단점 전부)
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	TArray<FText> DetailLines;

	// 돌파 칩 장점 — 토글과 무관하게 항상 표시
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	TArray<FText> BenefitLines;

	// 돌파 칩 단점 — 토글과 무관하게 항상 표시
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	TArray<FText> DrawbackLines;
};
