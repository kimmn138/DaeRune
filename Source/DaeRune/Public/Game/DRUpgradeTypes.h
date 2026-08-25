// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRUpgradeTypes.generated.h"

class UTexture2D;

/**
 * 업그레이드 칩이 변경하는 대상 스탯.
 * - Skill* 항목은 "특정 스킬" 단위라서 FDRUpgradeModifier::TargetAbilityTag 와 함께 쓴다.
 *   (TargetAbilityTag 가 비어 있으면 그 캐릭터의 모든 스킬에 적용)
 * - 나머지는 캐릭터 단위(전역) 스탯이라 TargetAbilityTag 를 무시한다.
 *
 * ContainerHealth 주의: 플레이어 체력은 컨테이너 시스템이라 MaxHealth 를 직접 올리면
 * 컨테이너 인덱스 계산과 오염 해제 로직(NumContainers * ContainerHealth)이 깨진다.
 * 따라서 체력 업그레이드는 "컨테이너 1칸 용량"을 올리는 방식으로만 정의한다. (Plan2.md 2.2 참조)
 */
UENUM(BlueprintType)
enum class EDRUpgradeStat : uint8
{
	// ===== 캐릭터 단위 (전역) =====
	ContainerHealth		UMETA(DisplayName = "컨테이너 체력"),
	MaxWater			UMETA(DisplayName = "최대 물"),
	MoveSpeed			UMETA(DisplayName = "이동 속도"),
	DamageTaken			UMETA(DisplayName = "받는 피해"),
	WaterGain			UMETA(DisplayName = "물 획득량"),

	// ===== 스킬 단위 (TargetAbilityTag 사용) =====
	SkillDamage			UMETA(DisplayName = "스킬 피해량"),
	SkillWaterCost		UMETA(DisplayName = "스킬 물 소모량"),
	SkillCooldown		UMETA(DisplayName = "스킬 딜레이"),
	SkillProjectileCount UMETA(DisplayName = "스킬 투사체 수"),

	Count UMETA(Hidden) // 배열 크기 계산용 - 항상 마지막
};

/**
 * 모디파이어 연산 방식.
 * 최종값 = (Base + 모든 Flat 합) * (1 + 모든 Percent 합)
 * 순서 의존성을 없애기 위해 두 누산기로만 합산한다.
 */
UENUM(BlueprintType)
enum class EDRUpgradeOp : uint8
{
	Flat		UMETA(DisplayName = "합연산(절대값)"),
	Percent		UMETA(DisplayName = "곱연산(비율, 0.1 = +10%)")
};

/**
 * 칩 분류. ★UI 분류·표기 전용이며 장착 제한이 아니다.★
 *
 * 슬롯에는 종류 구분이 없다 — 스탯 칩이든 돌파 칩이든 통합 6칸 어디에나 장착된다.
 * 이 enum 이 남아서 하는 일은 두 가지뿐이다:
 *   ① 업그레이드 화면 우측 목록의 필터 탭(STATS / ASCENSION)
 *   ② UDRChipCatalog::ValidateCatalog() 의 "단점 없는 돌파 칩" 경고
 * → 장착 판정 코드에서 Category 를 읽는 곳이 하나라도 생기면 3차 개정 규칙이 깨진 것이다.
 * (Plan2.md 4.4 참조)
 */
UENUM(BlueprintType)
enum class EDRChipCategory : uint8
{
	Stat		UMETA(DisplayName = "스탯 칩"),
	Ascension	UMETA(DisplayName = "돌파 칩"),

	Count UMETA(Hidden) // 카테고리 개수 계산용 - 항상 마지막
};

/** 재화 지급 사유 분류. 결과창 라벨/업적 화면 그룹핑에만 사용한다. */
UENUM(BlueprintType)
enum class EDRRewardKind : uint8
{
	StageFirstClear		UMETA(DisplayName = "스테이지 최초 클리어"),
	Achievement			UMETA(DisplayName = "업적"),
	ClearCondition		UMETA(DisplayName = "클리어 조건")
};

/** 슬롯 해금 / 칩 장착 요청의 결과 코드. UI 가 사유별 메시지를 띄우는 데 사용. */
UENUM(BlueprintType)
enum class EDRUpgradeResult : uint8
{
	Success,
	InvalidConfig,			// ProgressionConfig / ChipCatalog 미지정
	SystemLocked,			// 업그레이드 시스템 미해금 (스테이지1 최초 클리어 전)
	UnknownChip,			// 카탈로그에 없는 ChipId
	WrongClass,				// 이 로봇의 칩이 아님
	ChipLocked,				// 슬롯 해금 단계 미달 (RequiredSlotTier)
	AlreadyEquipped,		// 같은 칩 중복 장착 불가
	NotEquipped,			// 해제할 칩이 장착돼 있지 않음
	NotEnoughSlots,			// 남은 빈 슬롯이 필요 슬롯 수보다 적음
	NotEnoughCurrency,
	AllSlotsUnlocked,		// 더 해금할 슬롯이 없음
	NoSlotToRefund,			// 환불할 해금 슬롯이 없음
	RefundDisabled,			// 설정에서 슬롯 환불을 막아둠
	SlotOccupied,			// 환불 대상 슬롯이 칩에 점유 중 (먼저 칩을 해제해야 함)
	PreviousSlotLocked		// 아직 해금되지 않은 칸 (순차 해금 — 앞 칸을 먼저 해금해야 한다)
};

/**
 * 슬롯 1칸의 상태. 업그레이드 화면 좌측 6칸의 텍스처 스왑 기준이다.
 * 순차 해금이므로 Locked 칸은 항상 뒤쪽에 모인다. (Plan2.md 21.1 참조)
 */
UENUM(BlueprintType)
enum class EDRSlotState : uint8
{
	Locked		UMETA(DisplayName = "잠김"),
	Empty		UMETA(DisplayName = "빈 칸"),
	Occupied	UMETA(DisplayName = "장착됨")
};

/** 스탯이 "스킬 단위"인지 판정. */
FORCEINLINE bool DRIsSkillStat(EDRUpgradeStat Stat)
{
	return Stat == EDRUpgradeStat::SkillDamage
		|| Stat == EDRUpgradeStat::SkillWaterCost
		|| Stat == EDRUpgradeStat::SkillCooldown
		|| Stat == EDRUpgradeStat::SkillProjectileCount;
}

/**
 * 값이 "낮을수록 좋은" 스탯인가.
 * 미리보기(FDRStatPreviewLine::bIsWorse)와 단점 표기 색상을 판정하는 데 쓴다.
 * 예: 받는 피해 +40% 는 나쁜 변화지만, 이동 속도 +40% 는 좋은 변화다.
 */
FORCEINLINE bool DRIsLowerBetter(EDRUpgradeStat Stat)
{
	return Stat == EDRUpgradeStat::DamageTaken
		|| Stat == EDRUpgradeStat::SkillWaterCost
		|| Stat == EDRUpgradeStat::SkillCooldown;
}

/**
 * 칩 1개가 부여하는 효과 1건.
 * 장점/단점 모두 같은 구조로 표현하고, bIsDrawback 으로 UI 색상/분류만 구분한다.
 * (돌파 칩의 장점·단점은 세부 정보 토글 상태와 무관하게 항상 표시한다 — Plan2.md 10.1)
 */
USTRUCT(BlueprintType)
struct FDRUpgradeModifier
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	EDRUpgradeStat Stat = EDRUpgradeStat::ContainerHealth;

	// 스킬 단위 스탯일 때 대상 스킬(예: Abilities.GardenRobot.SeedCannon).
	// 비어 있으면 그 캐릭터의 모든 스킬에 적용된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (Categories = "Abilities"))
	FGameplayTag TargetAbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	EDRUpgradeOp Op = EDRUpgradeOp::Percent;

	// Flat = 절대값, Percent = 비율(0.1 → +10%). 음수면 감소.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	float Value = 0.f;

	// 단점 효과인가 (UI 에서 빨간색/경고 표기).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	bool bIsDrawback = false;

	// 상세 수치 문구를 직접 쓰고 싶을 때. 비어 있으면 "스탯명 ±수치" 를 자동 생성한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	FText DetailOverride;

	// "세부 정보 토글" 에서 보여줄 상세 수치 문구.
	FText GetDetailText() const;
};

/**
 * 업그레이드 칩 1개의 정의. UDRChipCatalog 데이터 에셋에 배열로 담긴다.
 * ChipId 는 세이브/복제 키이므로 한 번 정하면 변경 금지.
 */
USTRUCT(BlueprintType)
struct FDRUpgradeChipDefinition
{
	GENERATED_BODY()

	// 세이브/네트워크 키. 변경 시 기존 세이브의 장착 정보가 유실된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FName ChipId;

	// 어느 로봇의 칩인지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	EPlayerCharacterClass OwnerClass = EPlayerCharacterClass::Gardener;

	// UI 분류·표기 전용 (필터 탭). 어느 칸에 꽂히는지를 결정하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	EDRChipCategory Category = EDRChipCategory::Stat;

	// "적용 대상 스킬" 표기용. 비어 있으면 캐릭터 전체 대상.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (Categories = "Abilities"))
	FGameplayTag TargetAbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText DisplayName;

	// 이해하기 쉬운 효과 요약 (기본 표시 — 정확한 수치는 세부 정보 토글에서)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText EffectSummary;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UTexture2D> Icon;

	// 이 칩이 점유하는 슬롯 수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot", meta = (ClampMin = "1"))
	int32 RequiredSlotCount = 1;

	// 통합 슬롯을 이 개수 이상 해금해야 사용 가능 (슬롯 해금 단계 = 유일한 칩 해금 조건)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot", meta = (ClampMin = "1"))
	int32 RequiredSlotTier = 1;

	// 실제 효과(장점 + 단점). bIsDrawback 으로 구분.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TArray<FDRUpgradeModifier> Modifiers;

	// 단점 모디파이어가 하나라도 있는가 (UI 경고 아이콘용)
	bool HasDrawback() const
	{
		for (const FDRUpgradeModifier& Mod : Modifiers)
		{
			if (Mod.bIsDrawback) return true;
		}
		return false;
	}
};

/** 특정 스탯에 대해 합산된 결과. 최종값 = (Base + Flat) * (1 + Percent) */
USTRUCT(BlueprintType)
struct FDRResolvedStat
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	float Flat = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	float Percent = 0.f;

	float Apply(float Base) const { return (Base + Flat) * (1.f + Percent); }

	bool IsIdentity() const { return FMath::IsNearlyZero(Flat) && FMath::IsNearlyZero(Percent); }
};

/** 스킬 1개에 대한 스탯별 합산 결과 (index = (int32)EDRUpgradeStat) */
USTRUCT()
struct FDRSkillStatBlock
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FDRResolvedStat> Stats;

	void EnsureSize()
	{
		const int32 Num = static_cast<int32>(EDRUpgradeStat::Count);
		if (Stats.Num() != Num)
		{
			Stats.SetNum(Num);
		}
	}
};

/**
 * 장착된 칩 목록을 "즉시 조회 가능한 수치"로 펼쳐 놓은 런타임 캐시.
 * 서버/각 클라이언트가 복제된 칩 Id 배열로부터 각자 로컬에서 구축한다.
 * 비어 있으면 항등 함수이므로, 칩을 하나도 안 낀 상태는 업그레이드 도입 전과 완전히 동일하다.
 * (Plan2.md 4.6 참조)
 */
USTRUCT(BlueprintType)
struct FDRUpgradeRuntime
{
	GENERATED_BODY()

	// 캐릭터 단위 스탯 + "모든 스킬" 대상 스킬 스탯 (index = (int32)EDRUpgradeStat)
	UPROPERTY()
	TArray<FDRResolvedStat> GlobalStats;

	// 스킬 태그별 스탯
	UPROPERTY()
	TMap<FGameplayTag, FDRSkillStatBlock> SkillStats;

	void Reset()
	{
		GlobalStats.Reset();
		GlobalStats.SetNum(static_cast<int32>(EDRUpgradeStat::Count));
		SkillStats.Reset();
	}

	void EnsureSize()
	{
		const int32 Num = static_cast<int32>(EDRUpgradeStat::Count);
		if (GlobalStats.Num() != Num)
		{
			GlobalStats.SetNum(Num);
		}
	}

	// 모디파이어 1건 누적.
	void AddModifier(const FDRUpgradeModifier& Modifier)
	{
		if (FMath::IsNearlyZero(Modifier.Value)) return;

		EnsureSize();

		const int32 StatIndex = static_cast<int32>(Modifier.Stat);
		if (!GlobalStats.IsValidIndex(StatIndex)) return;

		// 스킬 단위 스탯 + 대상 태그 지정 → 태그별 블록에 누적
		if (DRIsSkillStat(Modifier.Stat) && Modifier.TargetAbilityTag.IsValid())
		{
			FDRSkillStatBlock& Block = SkillStats.FindOrAdd(Modifier.TargetAbilityTag);
			Block.EnsureSize();
			FDRResolvedStat& Target = Block.Stats[StatIndex];
			(Modifier.Op == EDRUpgradeOp::Flat ? Target.Flat : Target.Percent) += Modifier.Value;
			return;
		}

		// 전역 스탯 또는 "모든 스킬" 대상
		FDRResolvedStat& Target = GlobalStats[StatIndex];
		(Modifier.Op == EDRUpgradeOp::Flat ? Target.Flat : Target.Percent) += Modifier.Value;
	}

	// 캐릭터 단위 스탯 적용.
	float Apply(EDRUpgradeStat Stat, float Base) const
	{
		const int32 StatIndex = static_cast<int32>(Stat);
		if (!GlobalStats.IsValidIndex(StatIndex)) return Base;
		return GlobalStats[StatIndex].Apply(Base);
	}

	// 스킬 단위 스탯 적용. 전역("모든 스킬") 성분과 해당 태그 성분을 합산한다.
	float ApplySkill(const FGameplayTag& AbilityTag, EDRUpgradeStat Stat, float Base) const
	{
		const int32 StatIndex = static_cast<int32>(Stat);
		if (!GlobalStats.IsValidIndex(StatIndex)) return Base;

		FDRResolvedStat Combined = GlobalStats[StatIndex];
		if (AbilityTag.IsValid())
		{
			if (const FDRSkillStatBlock* Block = SkillStats.Find(AbilityTag))
			{
				if (Block->Stats.IsValidIndex(StatIndex))
				{
					Combined.Flat += Block->Stats[StatIndex].Flat;
					Combined.Percent += Block->Stats[StatIndex].Percent;
				}
			}
		}
		return Combined.Apply(Base);
	}

	// 캐릭터 단위 스탯의 합산 결과 조회 (미리보기 UI 용)
	FDRResolvedStat GetGlobalStat(EDRUpgradeStat Stat) const
	{
		const int32 StatIndex = static_cast<int32>(Stat);
		return GlobalStats.IsValidIndex(StatIndex) ? GlobalStats[StatIndex] : FDRResolvedStat();
	}

	bool IsEmpty() const
	{
		if (SkillStats.Num() > 0) return false;
		for (const FDRResolvedStat& Stat : GlobalStats)
		{
			if (!Stat.IsIdentity()) return false;
		}
		return true;
	}
};
