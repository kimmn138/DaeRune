// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRUpgradeTypes.h"
#include "DRProgressionTypes.generated.h"

/**
 * 한 로봇(클래스)의 업그레이드 슬롯/장착 상태. (세이브 저장 단위)
 *
 * 슬롯에는 종류 구분이 없다 — 구분 없는 통합 6칸을 앞에서부터 순차 해금한다.
 * 칸 배열(SlotChips)은 UI 의 01~06 칸과 1:1 대응하며, 여러 칸을 점유하는 칩
 * (RequiredSlotCount >= 2)은 같은 ChipId 가 여러 칸에 기록된다.
 * → 점유 칸 수 = 그 Id 의 등장 횟수. 자료구조만으로 정합성이 유지된다.
 *
 * 불변식: SlotChips[0 .. UnlockedSlots-1] 이 사용 가능 칸, 그 뒤는 잠긴 칸이다.
 * (Plan2.md 4.2 참조)
 */
USTRUCT(BlueprintType)
struct FDRClassUpgradeState
{
	GENERATED_BODY()

	// 해금한 슬롯 수 (0 .. MaxSlots). 카테고리 구분 없는 통합 해금 수.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrade")
	int32 UnlockedSlots = 0;

	// 칸별 점유 칩 (길이 = MaxSlots, 빈 칸은 NAME_None)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<FName> SlotChips;

	// 슬롯 해금에 지불한 총액 (환불 상한)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrade")
	int32 SpentCurrency = 0;

	// 해금된 칸 중 비어 있는 칸 수
	int32 CountFreeSlots() const
	{
		const int32 Unlocked = FMath::Min(UnlockedSlots, SlotChips.Num());

		int32 Free = 0;
		for (int32 Index = 0; Index < Unlocked; ++Index)
		{
			if (SlotChips[Index].IsNone())
			{
				++Free;
			}
		}
		return Free;
	}

	// 그 칩이 점유한 칸 수 (0 = 미장착)
	int32 CountOccupiedSlots(FName ChipId) const
	{
		if (ChipId.IsNone()) return 0;

		int32 Count = 0;
		for (const FName& Slot : SlotChips)
		{
			if (Slot == ChipId)
			{
				++Count;
			}
		}
		return Count;
	}

	// 장착된 모든 칩 Id (중복 없이). 서버 보고/런타임 해석용.
	void GetEquippedChips(TArray<FName>& OutChips) const
	{
		OutChips.Reset();
		for (const FName& Slot : SlotChips)
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

	// 이해하기 쉬운 효과 요약 (툴팁/목록의 한 줄 설명)
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	FText EffectSummary;

	// 칩 카드에 크게 찍는 2행 짧은 라벨 ("이동 속도" / "+6%").
	// EffectSummary 는 문장이라 120px 카드에 안 들어간다 — 카드는 이 두 줄만 쓴다.
	// (슬롯 카드의 FDRSlotViewModel::ShortLabel* 과 같은 규칙으로 생성된다)
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	FText ShortLabelTop;

	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	FText ShortLabelBottom;

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

/**
 * 슬롯 1칸을 그리는 데 필요한 모든 정보. (FDRChipViewModel 의 슬롯 버전)
 *
 * UI 가 GetSlotAssignments() + 카탈로그를 직접 조합하지 않게 하기 위한 뷰모델이다.
 * 다중 칸 칩은 같은 ChipId 가 여러 칸에 나타나므로, 시각적으로 묶어 그릴 수 있도록
 * GroupSize / GroupOrder 를 함께 채운다. (Plan2.md 21.1 참조)
 */
USTRUCT(BlueprintType)
struct FDRSlotViewModel
{
	GENERATED_BODY()

	// 0 .. MaxSlots-1 (UI 의 01~06 칸에 1:1 대응)
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	EDRSlotState State = EDRSlotState::Locked;

	// ===== State == Occupied 일 때 채워진다 =====

	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FName ChipId;

	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FText ChipName;

	// 슬롯 카드용 짧은 2행 라벨 (예: "ATK" / "+5")
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FText ShortLabelTop;

	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FText ShortLabelBottom;

	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	TObjectPtr<UTexture2D> ChipIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	bool bHasDrawback = false;

	// 이 칩이 점유한 총 칸 수 (= RequiredSlotCount). 1이면 묶음 표시가 필요 없다.
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	int32 GroupSize = 0;

	// 묶음 안에서 이 칸이 몇 번째인가 (0-base). 칸이 흩어져 있어도 순서는 보존된다.
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	int32 GroupOrder = 0;

	// ===== State == Locked 일 때 채워진다 =====

	// 이 칸의 해금 비용 (-1 = 비용 미정/범위 밖)
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	int32 UnlockCost = -1;

	// 순차 해금상 "지금 살 수 있는" 칸인가 (잠긴 칸 중 가장 앞)
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	bool bIsNextUnlockable = false;

	// 해금 버튼 비활성 사유. bIsNextUnlockable == false 면 PreviousSlotLocked.
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	EDRUpgradeResult UnlockBlockReason = EDRUpgradeResult::Success;
};

/**
 * "이 칩을 끼면/빼면 무엇이 얼마나 변하는가" 1줄. (Plan2.md 21.4 참조)
 *
 * 절대값(예: 최대 체력 400 → 480)이 아니라 ★증분(델타)★ 표기다.
 * 절대값을 내려면 클래스별 PrimaryAttributes GE 의 기본값까지 해석해야 해서 비용이 크다.
 */
USTRUCT(BlueprintType)
struct FDRStatPreviewLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Preview")
	EDRUpgradeStat Stat = EDRUpgradeStat::ContainerHealth;

	// 스킬 단위 스탯일 때 대상 스킬. 비어 있으면 캐릭터 전역.
	UPROPERTY(BlueprintReadOnly, Category = "Preview")
	FGameplayTag SkillTag;

	// "컨테이너 체력" / "씨앗 대포 · 스킬 피해량"
	UPROPERTY(BlueprintReadOnly, Category = "Preview")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Preview")
	float CurrentFlat = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Preview")
	float CurrentPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Preview")
	float PreviewFlat = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Preview")
	float PreviewPercent = 0.f;

	// "+12%" / "-15" / "+20, +12%"
	UPROPERTY(BlueprintReadOnly, Category = "Preview")
	FText DeltaText;

	// 나빠지는 변화인가 (빨간색 표기용). DRIsLowerBetter() 를 반영한 판정이다.
	UPROPERTY(BlueprintReadOnly, Category = "Preview")
	bool bIsWorse = false;
};

/**
 * ListView / TileView 로 칩 목록을 돌릴 때 쓰는 래퍼 오브젝트.
 * (UMG 리스트 계열은 UObject* 만 아이템으로 받는다 — USTRUCT 는 못 넣는다.)
 *
 * 지금은 UniformGridPanel 로 직접 그려도 되지만, 칩이 늘어나 가상화가 필요해지면
 * 이 클래스로 교체하는 비용이 1줄이 되도록 미리 정의해 둔다. (Plan2.md 19.6)
 */
UCLASS(BlueprintType)
class DAERUNE_API UDRChipViewModelObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Chip")
	FDRChipViewModel Data;
};
