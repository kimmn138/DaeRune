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

	// ===== Plan3(공용칩) 추가 =====
	// ★신규 값은 반드시 Count 바로 앞에 붙인다.★ 기존 값 사이에 끼워 넣으면
	// DA_ChipCatalog 에 직렬화된 기존 칩의 효과가 다른 스탯으로 밀린다.
	// (ECharacterClass 의 MoleBoss 주석과 같은 규칙 — Plan3(공용칩) §4.1 D-4)

	// 캐릭터 단위. ★Flat 전용★ — 컨테이너 1칸 용량이 아니라 "총 최대 체력(HP)" 증가량이다.
	// ADRCharacter::RefreshUpgradeEffects() 가 NumContainers 로 나눠 칸 용량에 분배하므로
	// 컨테이너 불변식(MaxHealth = NumContainers × ContainerHealth)이 유지된다.
	// 비율 증가가 필요하면 ContainerHealth 쪽에 Percent 로 쓴다(수학적으로 동일, 이중 정의 방지).
	MaxHealth			UMETA(DisplayName = "최대 체력"),

	// 캐릭터 단위. 전투 이탈 → 자동 체력 회복이 시작되기까지의 대기 시간(초). 음수 = 단축.
	HealthRegenDelay	UMETA(DisplayName = "회복 시작 시간"),

	// 캐릭터 단위. 이 캐릭터가 ★주는★ 모든 피해의 배율.
	// UExecCalc_Damage 의 소스측에서 적용되므로 스킬/투사체/디버프 DoT 을 전부 포함한다.
	// (SkillDamage 는 어빌리티 단위라 Stages[] 처럼 그 경로를 안 타는 피해원이 새는 것과 대비
	//  — Plan3(공용칩) §17.2.1)
	DamageDealt			UMETA(DisplayName = "주는 피해"),

	// ===== Plan5(씨앗칩) 추가 =====
	// ★신규 값은 반드시 Count 바로 앞에 붙인다.★ 기존 값 사이에 끼우면
	// DA_ChipCatalog 에 직렬화된 기존 칩의 효과가 다른 스탯으로 밀린다. (Plan5(씨앗칩) D-4)

	// 스킬 단위. 그 스킬이 ★아군에게 주는 회복량★ 의 배율/가감.
	// 씨앗 폭탄처럼 내부/외부 두 값을 가진 스킬이면 양쪽에 똑같이 적용한다.
	SkillHealing		UMETA(DisplayName = "스킬 회복량"),

	// 스킬 단위. 그 스킬의 ★효과 범위(반경)★ 배율/가감. 단위는 언리얼 단위(uu).
	// 내부/외부 반경이 나뉜 스킬이면 양쪽에 똑같이 적용한다.
	SkillAreaRadius		UMETA(DisplayName = "스킬 범위"),

	// ===== Plan5(물대포칩) 추가 =====
	// ★신규 값은 반드시 Count 바로 앞에 붙인다.★ (Plan5(물대포칩) D-17)

	// 스킬 단위. 그 스킬이 ★아군에게 지급하는 물의 양★ 의 배율/가감.
	// ⚠️ WaterGain(캐릭터 단위, ★받는 쪽★)과 혼동하지 말 것 — 이쪽은 ★주는 쪽★ 스킬의 지급량이다.
	//    둘은 각자 곱해져서 최종 지급량이 된다.
	SkillWaterGrant		UMETA(DisplayName = "스킬 물 지급량"),

	// ===== Plan5(발톱칩) 추가 =====
	// ★신규 값은 반드시 Count 바로 앞에 붙인다.★ 기존 값 사이에 끼우면
	// DA_ChipCatalog 에 직렬화된 기존 칩의 효과가 다른 스탯으로 밀린다.

	/**
	 * 스킬 단위. 그 스킬의 ★선딜레이(시전 준비 시간) 배율★.
	 *
	 * ★단위가 "초"가 아니라 "배율"이다.★ 기준값 1.0 에 Apply 해서 쓴다 —
	 *     WindupScale = ApplySkill(SkillWindup, 1.0f);   // 칩이 없으면 정확히 1.0
	 *     PlayRate    = BasePlayRate / WindupScale;      // 3.0 이면 1/3 속도
	 * 선딜의 실제 길이는 몽타주 에셋 안에 있어 어빌리티가 초 단위로 알 수 없기 때문이다.
	 *
	 * ⚠️ 그래서 DRIsTimeStat 에는 ★넣지 않는다★ — 넣으면 UI 가 "+200초" 로 표시한다.
	 */
	SkillWindup			UMETA(DisplayName = "스킬 선딜레이"),

	// ===== Plan5(동전칩) 추가 =====
	// ★신규 값은 반드시 Count 바로 앞에 붙인다.★ 기존 값 사이에 끼우면
	// DA_ChipCatalog 에 직렬화된 기존 칩의 효과가 다른 스탯으로 밀린다.

	/**
	 * 스킬 단위. 연사형 스킬의 ★발사 간격(초)★. 짧을수록 좋다.
	 *
	 *     간격 = ApplySkill(SkillFireInterval, BaseFireInterval) ÷ 공격 속도 버프 배율
	 *
	 * ★SkillWindup 과 달리 단위가 "초"다.★ 발사 간격은 몽타주가 아니라 GA 프로퍼티라서
	 * 어빌리티가 기준값을 초 단위로 정확히 안다. 그래서 DRIsTimeStat 에 ★넣는다★.
	 *
	 * ⚠️ SkillCooldown 을 재사용하지 않는다 — 그 축은 ApplyCooldown 이 쿨다운 GE 에 쓴다.
	 *    기본 공격에 쿨다운 GE 가 붙는 순간 같은 칩이 이중으로 걸린다. (Plan5(동전칩) D-37)
	 */
	SkillFireInterval	UMETA(DisplayName = "스킬 발사 간격"),

	/**
	 * 스킬 단위. 그 스킬이 만드는 ★투사체의 크기 배율★ — 외형과 충돌 구(판정)가 함께 커진다.
	 *
	 * ★단위가 "배율"이다.★ 기준값 1.0 에 Apply 해서 스폰 트랜스폼의 Scale3D 로 쓴다 —
	 *     Scale = ApplySkill(SkillProjectileScale, 1.0f);   // 칩이 없으면 정확히 1.0
	 * 투사체의 실제 반경은 발사체 BP(Sphere)에 있어 어빌리티가 모르기 때문이다(SkillWindup 과 같은 이유).
	 *
	 * ⚠️ SkillAreaRadius(효과 반경)와 다른 축이다 — 투사체 몸통 ≠ 폭발·판정 반경. (Plan5(동전칩) D-40)
	 */
	SkillProjectileScale	UMETA(DisplayName = "스킬 투사체 크기"),

	// ===== Plan5(공기탄칩) 추가 =====
	// ★신규 값은 반드시 Count 바로 앞에 붙인다.★ 기존 값 사이에 끼우면
	// DA_ChipCatalog 에 직렬화된 기존 칩의 효과가 다른 스탯으로 밀린다. (Plan5 T-30)

	/**
	 * 스킬 단위. 차지형 스킬의 ★단계당 증가분 배율★ — 기준 1.0 에 Apply 해서 쓴다.
	 *     단계 피해 = 0단계 + (그 단계 − 0단계) × ApplySkill(SkillChargeGrowth, 1.0)
	 * 0단계(즉발)는 바뀌지 않는다. 배율이라 DRIsTimeStat · DRIsLowerBetter 에 ★넣지 않는다★ (Plan5 D-100 · T-103)
	 */
	SkillChargeGrowth		UMETA(DisplayName = "스킬 차지 증가량"),

	// ===== Plan5(물점프칩) 추가 =====

	/**
	 * 스킬 단위. 판정이 두 겹(내부 강판정 / 외부 약판정)인 스킬의 ★외부(약판정) 피해에만★ 곱하는 배율.
	 * SkillDamage 는 양쪽에 걸리고, 이 축은 그 위에 외부에만 한 번 더 곱해진다 —
	 *     외부 피해 = ApplySkill(SkillOuterDamage, ApplySkill(SkillDamage, OuterDamage))
	 * ⚠️ 지금은 물 점프만 읽는다. 씨앗 폭탄 등에 쓰려면 그 스킬도 읽게 해야 한다 (Plan5 D-104)
	 */
	SkillOuterDamage		UMETA(DisplayName = "스킬 외곽 피해"),

	/**
	 * 스킬 단위. 점프 · 발사형 이동기의 ★실제 최고 높이 배율★ — 기준 1.0 에 Apply 해서 쓴다.
	 * ★높이는 속도의 제곱에 비례★ 하므로 임펄스에는 √배율을 곱한다 —
	 *     임펄스 = BaseImpulse × Sqrt(ApplySkill(SkillLaunchHeight, 1.0))   // 0.7 → ×0.837 → 높이 ×0.70
	 * 배율이라 DRIsTimeStat 에 ★넣지 않는다★ (Plan5 D-105 · T-109)
	 */
	SkillLaunchHeight		UMETA(DisplayName = "스킬 상승 높이"),

	/**
	 * 스킬 단위. 공중(떨어지는 중)에서 그 스킬을 쓸 수 있는 ★횟수★. 착지하면 다시 채워진다.
	 * Flat 으로 쓴다(+1 = 한 번 더). 정수 — GA 가 반올림해 읽는다. (Plan5 D-107)
	 */
	SkillAirUseCount		UMETA(DisplayName = "스킬 공중 사용 횟수"),

	// ===== Plan5(돌진칩) 추가 =====

	/**
	 * 스킬 단위. 그 스킬이 ★시전자 자신에게 주는 피해(자해)★ 의 배율 — 낮을수록 좋다.
	 *     자해 = ApplySkill(SkillSelfDamage, 기본 자해량)   // +1.0 → 50 → 100
	 * DRIsLowerBetter 에 ★넣는다★ (안 넣으면 "+100%" 가 초록으로 뜬다 — Plan5 T-130) (Plan5 D-114)
	 */
	SkillSelfDamage			UMETA(DisplayName = "스킬 자해 피해"),

	/**
	 * 스킬 단위. 그 스킬이 시전자에게 주는 ★이동 속도 증가분(칸 · 스택당)★ 의 배율.
	 *     칸당 증가 = ApplySkill(SkillSpeedBonus, 0.2)   // −0.3 → 0.14 → GE 배율 1.14
	 * 배율 자체가 아니라 "증가분"에 곱한다 — 1.2 에 곱하면 1.2 × 0.7 = 0.84(감속)가 된다 (Plan5 D-118)
	 */
	SkillSpeedBonus			UMETA(DisplayName = "스킬 이속 버프"),

	// ===== Plan5(가방칩 · 도발칩 · 모자칩) 추가 =====
	// ★신규 값은 반드시 Count 바로 앞에 붙인다.★ 기존 값 사이에 끼우면
	// DA_ChipCatalog 에 직렬화된 기존 칩의 효과가 다른 스탯으로 밀린다. (Plan5 T-30)

	/**
	 * 스킬 단위. ★연속 공격★ 의 선딜레이 배율 — 기준 1.0 에 Apply 해서 쓴다(SkillWindup 과 같은 배율 축).
	 * 연속 공격이 있는 스킬(지금은 벨보이 기본 공격만)에서 SkillWindup 은 ★첫 타★ 에만, 이 축은 ★연속★ 에만 곱한다.
	 * 첫 타와 연속을 같은 비율로 바꾸려면 두 축에 같은 값을 넣는다 (Plan5 D-121 · T-136)
	 */
	SkillComboWindup		UMETA(DisplayName = "스킬 연속 선딜레이"),

	/** 스킬 단위. ★연속 공격★ 의 후딜레이 배율 — 기준 1.0. 첫 타 후딜에는 곱하지 않는다 (Plan5 D-121) */
	SkillComboRecovery		UMETA(DisplayName = "스킬 연속 후딜레이"),

	/**
	 * 스킬 단위. 그 스킬이 남기는 효과의 ★지속 시간(초)★ — 기준 초에 바로 Apply 한다(SkillFireInterval 과 같은 "초" 축).
	 *     적 도발 시간 = ApplySkill(SkillDuration, TauntDuration)   // Flat −2 → 5 → 3초
	 * 길수록 좋다 — DRIsTimeStat 에는 넣고 DRIsLowerBetter 에는 ★넣지 않는다★ (Plan5 D-132 · T-146)
	 * ⚠️ 지금은 벨보이 도발 시간 · 바닥 모자 수명만 읽는다
	 */
	SkillDuration			UMETA(DisplayName = "스킬 지속 시간"),

	Count UMETA(Hidden) // 배열 크기 계산용 - 항상 마지막
};

/**
 * 모디파이어 연산 방식.
 * 최종값 = (Base + 모든 Flat 합) * (모든 Percent 의 곱)
 *
 * ★Plan5(씨앗칩)★ Percent 는 ★곱연산★ 이다(예전에는 합산이었다).
 * 합산이면 감소 칩 두 개(-80% / -40%)가 -120% 가 되어 배율이 음수로 뒤집힌다 —
 * 음수 피해는 곧 회복이라 적 체력이 차오른다. 곱연산은 구조적으로 그 상태가 될 수 없다.
 * 곱셈도 교환법칙이 성립하므로 "순서 의존성 없음" 이라는 원래 설계 목표는 그대로 유지된다.
 */
UENUM(BlueprintType)
enum class EDRUpgradeOp : uint8
{
	Flat		UMETA(DisplayName = "합연산(절대값)"),

	// 입력은 "비율"이지만 누산은 곱연산이다.
	// 같은 스탯에 두 칩이 +0.5 를 걸면 ×2.0 이 아니라 ×2.25 가 된다.
	Percent		UMETA(DisplayName = "곱연산(비율, 0.1 → ×1.1)")
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
	PreviousSlotLocked,		// 아직 해금되지 않은 칸 (순차 해금 — 앞 칸을 먼저 해금해야 한다)

	// ★Plan5(스킬당1칩)★ 같은 스킬 칩이 이미 장착돼 있다 — 그 칩이 있는 칸에 드롭하면 교체된다.
	// ★반드시 맨 끝에 붙인다★ — 중간에 끼우면 BP 의 Switch 핀·바이트 값이 밀린다 (Plan5 D-52)
	SkillConflict
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

/**
 * 스탯이 "스킬 단위"인지 판정.
 * Plan3(공용칩) 의 MaxHealth / HealthRegenDelay 는 둘 다 캐릭터 단위라 여기에 넣지 않는다.
 */
FORCEINLINE bool DRIsSkillStat(EDRUpgradeStat Stat)
{
	return Stat == EDRUpgradeStat::SkillDamage
		|| Stat == EDRUpgradeStat::SkillWaterCost
		|| Stat == EDRUpgradeStat::SkillCooldown
		|| Stat == EDRUpgradeStat::SkillProjectileCount
		|| Stat == EDRUpgradeStat::SkillHealing			// Plan5(씨앗칩)
		|| Stat == EDRUpgradeStat::SkillAreaRadius		// 〃
		|| Stat == EDRUpgradeStat::SkillWaterGrant		// Plan5(물대포칩)
		|| Stat == EDRUpgradeStat::SkillWindup			// Plan5(발톱칩)
		|| Stat == EDRUpgradeStat::SkillFireInterval	// Plan5(동전칩)
		|| Stat == EDRUpgradeStat::SkillProjectileScale	// 〃
		|| Stat == EDRUpgradeStat::SkillChargeGrowth	// Plan5(공기탄칩)
		|| Stat == EDRUpgradeStat::SkillOuterDamage		// Plan5(물점프칩)
		|| Stat == EDRUpgradeStat::SkillLaunchHeight	// 〃
		|| Stat == EDRUpgradeStat::SkillAirUseCount		// 〃
		|| Stat == EDRUpgradeStat::SkillSelfDamage		// Plan5(돌진칩)
		|| Stat == EDRUpgradeStat::SkillSpeedBonus		// 〃
		|| Stat == EDRUpgradeStat::SkillComboWindup		// Plan5(가방칩)
		|| Stat == EDRUpgradeStat::SkillComboRecovery	// 〃
		|| Stat == EDRUpgradeStat::SkillDuration;		// Plan5(도발칩 · 모자칩) — 빠뜨리면 도발 −2초가 바닥 모자에도 걸린다 (T-145)
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
		|| Stat == EDRUpgradeStat::SkillCooldown
		|| Stat == EDRUpgradeStat::HealthRegenDelay
		|| Stat == EDRUpgradeStat::SkillWindup		// Plan5(발톱칩) 선딜은 짧을수록 좋다
		|| Stat == EDRUpgradeStat::SkillFireInterval	// Plan5(동전칩) 발사 간격도 짧을수록 좋다
		|| Stat == EDRUpgradeStat::SkillSelfDamage	// Plan5(돌진칩) 자해는 적을수록 좋다
		|| Stat == EDRUpgradeStat::SkillComboWindup	// Plan5(가방칩) 연속 선딜 · 후딜도 짧을수록 좋다 (지속 시간은 길수록 좋아 제외 — T-146)
		|| Stat == EDRUpgradeStat::SkillComboRecovery;
}

/**
 * 값의 단위가 "초"인가. 표기에 단위를 붙이는 데만 쓴다.
 *
 * 이게 없으면 회복 시작 시간 -0.5 가 화면에 "-0.5" 로만 나와서
 * 비율(-0.5%)인지 시간(-0.5초)인지 구분이 안 된다. (Plan3(공용칩) §8.3)
 */
FORCEINLINE bool DRIsTimeStat(EDRUpgradeStat Stat)
{
	return Stat == EDRUpgradeStat::SkillCooldown
		|| Stat == EDRUpgradeStat::HealthRegenDelay
		|| Stat == EDRUpgradeStat::SkillFireInterval	// Plan5(동전칩) 단위가 초다 (배율인 SkillWindup 은 제외 — T-25)
		|| Stat == EDRUpgradeStat::SkillDuration;		// Plan5(도발칩) 단위가 초다 — Flat −2 가 "−2초"로 표기 (배율인 연속 선딜 · 후딜은 제외)
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

	// 어느 로봇의 칩인지. bCommonChip == true 면 무시된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	EPlayerCharacterClass OwnerClass = EPlayerCharacterClass::Gardener;

	/**
	 * ★Plan3(공용칩)★ true 면 OwnerClass 를 무시하고 ★모든 로봇★의 목록에 나타난다.
	 *
	 * "공용"은 ★사용 가능 여부★ 이지 ★장착 공유★ 가 아니다 —
	 * 세이브가 로봇별 FDRClassUpgradeState 라서 정원로봇에 끼운 공용 칩이
	 * 자판기에 자동으로 끼워지지는 않는다. (Plan3(공용칩) §1.1 D-1)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	bool bCommonChip = false;

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

	/**
	 * ★Plan3(공용칩)★ 수치가 아닌 "기능 스위치".
	 * 장착 중이면 FDRUpgradeRuntime::GrantedTags 에 합쳐진다.
	 *
	 * 예: Upgrade.Feature.HUD.Vitals — 이 태그가 있어야 체력/물 HUD 가 보인다.
	 * ★GAS 의 어빌리티/이펙트 태그와는 무관하다.★ ASC 에 부여되지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect",
		meta = (Categories = "Upgrade.Feature"))
	FGameplayTagContainer GrantedTags;

	// 단점 모디파이어가 하나라도 있는가 (UI 경고 아이콘용)
	bool HasDrawback() const
	{
		for (const FDRUpgradeModifier& Mod : Modifiers)
		{
			if (Mod.bIsDrawback) return true;
		}
		return false;
	}

	/**
	 * ★이 칩이 그 로봇의 것인가 — 모든 소속 판정의 단일 진입점.★
	 *
	 * Chip.OwnerClass == SomeClass 로 직접 비교하는 코드를 남기지 말 것.
	 * 목록/해석/정화/검증 중 한 곳만 빠뜨려도 "장착은 되는데 다음 실행에 사라지는"
	 * 형태로 증상이 늦게 나타난다. (Plan3(공용칩) §5)
	 */
	bool IsUsableBy(EPlayerCharacterClass InClass) const
	{
		return bCommonChip || OwnerClass == InClass;
	}

	/**
	 * ★Plan5(스킬당1칩)★ "같은 스킬 칩은 동시에 1개만" 규칙의 묶음 키. 비어 있으면 제한 없음.
	 *
	 * 칩 레벨 TargetAbilityTag 그대로다 — 칩 카드의 "적용 대상 스킬" 표기와 같은 값이라
	 * 플레이어가 보는 것과 규칙이 일치한다 (Plan5 D-47). 공용 칩은 항상 제한 없음.
	 * 판정은 UDRChipCatalog::FindSkillConflict() 하나로만 한다 (D-49).
	 */
	FGameplayTag GetExclusiveSkillTag() const
	{
		return bCommonChip ? FGameplayTag() : TargetAbilityTag;
	}

	// 수치든 스위치든 효과가 하나라도 있는가 (ValidateCatalog 용).
	// HUD 표시 칩처럼 Modifiers 가 비어 있고 GrantedTags 만 있는 칩이 정상이다.
	bool HasAnyEffect() const
	{
		return Modifiers.Num() > 0 || GrantedTags.Num() > 0;
	}
};

/** 특정 스탯에 대해 합산된 결과. 최종값 = (Base + Flat) * Mult */
USTRUCT(BlueprintType)
struct FDRResolvedStat
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	float Flat = 0.f;

	/**
	 * ★Plan5(씨앗칩)★ 곱연산 누산기. ★기본값이 0 이 아니라 1 이다.★
	 *
	 * 칩마다 (1 + Modifier.Value) 를 ★곱해서★ 넣는다.
	 *   예) -80% 칩 + -40% 칩  →  0.2 × 0.6 = 0.12
	 *
	 * 합산(Percent += Value)이었을 때는 두 감소 칩이 -120% 가 되어 배율이 ★음수★ 가 됐다.
	 * 음수 피해는 곧 회복이라 적 체력이 차오른다 — 곱연산은 구조적으로 그 상태가 될 수 없다.
	 *
	 * ★이름을 Percent 에서 바꾼 이유★: 기본값이 0 이 아니라 1 이라는 사실을 놓치면
	 * `1.f + Stat.Percent` 같은 기존 관용구가 조용히 2배가 된다.
	 * 이름을 바꿔 모든 사용처가 ★컴파일 에러★ 로 드러나게 했다. (Plan5(씨앗칩) D-7)
	 *
	 * ⚠️ 이 구조체를 FMemory::Memzero 하거나 {0} 으로 초기화하면 모든 수치가 0 이 된다.
	 *    TArray::SetNum() 은 기본 생성자를 부르므로 안전하다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	float Mult = 1.f;

	float Apply(float Base) const { return (Base + Flat) * Mult; }

	bool IsIdentity() const { return FMath::IsNearlyZero(Flat) && FMath::IsNearlyEqual(Mult, 1.f); }
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

	// ★Plan3(공용칩)★ 장착 칩들이 부여한 기능 스위치의 합집합.
	UPROPERTY()
	FGameplayTagContainer GrantedTags;

	void Reset()
	{
		GlobalStats.Reset();
		GlobalStats.SetNum(static_cast<int32>(EDRUpgradeStat::Count));
		SkillStats.Reset();
		// ★이 줄을 빠뜨리면 칩을 빼도 HUD 가 계속 보인다★ (재구축이 누적만 하게 됨)
		GrantedTags.Reset();
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
		// Percent 의 Value 0 은 배율 1.0 이라 어차피 항등이다.
		if (FMath::IsNearlyZero(Modifier.Value)) return;

		EnsureSize();

		const int32 StatIndex = static_cast<int32>(Modifier.Stat);
		if (!GlobalStats.IsValidIndex(StatIndex)) return;

		// 누산 대상 결정 — 스킬 단위 스탯 + 대상 태그 지정이면 태그별 블록, 아니면 전역
		FDRResolvedStat* Target = nullptr;
		if (DRIsSkillStat(Modifier.Stat) && Modifier.TargetAbilityTag.IsValid())
		{
			FDRSkillStatBlock& Block = SkillStats.FindOrAdd(Modifier.TargetAbilityTag);
			Block.EnsureSize();
			Target = &Block.Stats[StatIndex];
		}
		else
		{
			Target = &GlobalStats[StatIndex];
		}

		// ★Plan5(씨앗칩)★ Percent 는 ★곱해서★ 넣는다 (예전에는 더해서 넣었다).
		if (Modifier.Op == EDRUpgradeOp::Flat)
		{
			Target->Flat += Modifier.Value;
		}
		else
		{
			Target->Mult *= (1.f + Modifier.Value);
		}
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
					// ★Plan5(씨앗칩)★ 전역("모든 스킬") 배율과 이 스킬 전용 배율을 곱한다.
					Combined.Mult *= Block->Stats[StatIndex].Mult;
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

	// ★Plan3(공용칩)★ 기능 스위치 조회. 호출부가 GrantedTags 를 직접 만지지 않게 한다.
	bool HasFeature(const FGameplayTag& FeatureTag) const
	{
		return FeatureTag.IsValid() && GrantedTags.HasTag(FeatureTag);
	}

	/**
	 * ★"수치상" 항등인가.★ GrantedTags 는 보지 않는다.
	 *
	 * ADRCharacter::RefreshUpgradeEffects() 가 "스탯 GE 를 얹을 필요가 있나"를 판단하는 데 쓴다.
	 * HUD 표시 칩만 끼운 상태는 어트리뷰트가 하나도 안 바뀌므로 GE 를 안 얹는 것이 맞다.
	 */
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
