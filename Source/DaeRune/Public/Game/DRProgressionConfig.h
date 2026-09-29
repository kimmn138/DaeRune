// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/DRUpgradeTypes.h"
#include "DRProgressionConfig.generated.h"

class UDRChipCatalog;
class UDRCosmeticCatalog;
class UGameplayEffect;

/**
 * ★Plan5(씨앗칩)★ "일정 시간 이동 속도를 배로 바꾸는 효과" 한 벌.
 * 감속/가속이 구조가 같아 재사용 가능하게 뽑아 뒀다. 다른 스킬 칩도 이걸 그대로 쓴다.
 */
USTRUCT(BlueprintType)
struct FDRTimedSpeedEffect
{
	GENERATED_BODY()

	/**
	 * 적용할 GE. 요구 사양:
	 *   - Duration Policy = Has Duration, Duration Magnitude = SetByCaller(Data.Duration)
	 *   - Modifier: MoveSpeed / Multiplicitive / SetByCaller(Data.MoveSpeed.Mult)
	 *   - Stacking = Aggregate by Source, Stack Limit 1,
	 *                Refresh Duration on Successful Application
	 *     (같은 시전자가 연속으로 맞혀도 중첩되지 않고 지속시간만 갱신된다)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpeedEffect")
	TSoftClassPtr<UGameplayEffect> Effect;

	// 변화량. 0.3 = ±30%. 실제 배율은 호출부가 (1 - M) 또는 (1 + M) 으로 만든다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpeedEffect",
		meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float Magnitude = 0.30f;

	// 지속시간(초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpeedEffect", meta = (ClampMin = "0.01"))
	float Duration = 5.0f;
};

/**
 * ★Plan5(씨앗칩)★ 정원로봇 씨앗 폭탄 칩 3종의 동적 상수.
 *
 * 카탈로그의 Modifiers 로 표현할 수 없는 값만 여기 둔다 —
 * "지불한 물의 몇 %를 돌려주는가", "몇 초 동안", "어떤 GE 로" 같은 것들이다.
 * 정적 수치(회복 ×2, 범위 +50%, 피해 -80% …)는 전부 DA_ChipCatalog 에 있다.
 */
USTRUCT(BlueprintType)
struct FDRSeedBombChipTuning
{
	GENERATED_BODY()

	// ========== ① GR.Asc.SeedRefund ==========

	// 직격 시 돌려받는 비율. 1.0 = 사용한 물 전액.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Refund", meta = (ClampMin = "0.0"))
	float DirectHitRefundRatio = 1.0f;

	/**
	 * 환급에 쓰는 GE. Instant + Water 에 SetByCaller(Water.SetByCaller.Grant) Additive.
	 * 기존 GE_WaterGrant(적 처치 물 지급용)를 그대로 지정하면 된다 — 요구 스펙이 같다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Refund")
	TSoftClassPtr<UGameplayEffect> RefundEffect;

	// ========== ② GR.Asc.SeedSupport ==========

	// 폭발에 피해를 입은 적에게 거는 감속 (기본 30% 감소 / 5초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Support")
	FDRTimedSpeedEffect EnemySlow;

	// 폭발에 회복을 받은 아군에게 거는 가속 (기본 30% 증가 / 5초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Support")
	FDRTimedSpeedEffect AllyHaste;
};

/**
 * ★Plan5(물대포칩)★ 정원로봇 물대포 칩 3종의 동적 상수.
 *
 * 카탈로그의 Modifiers 로 표현할 수 없는 값만 여기 둔다 —
 * "스택 1개당 몇 %인가", "몇 초 동안", "어떤 GE 로" 같은 것들이다.
 * 정적 수치(피해 0, 급수 ×2, 물 소모 -80% …)는 전부 DA_ChipCatalog 에 있다.
 */
USTRUCT(BlueprintType)
struct FDRWaterPumpChipTuning
{
	GENERATED_BODY()

	// ========== ① GR.Asc.PumpRamp (가압 노즐) ==========

	/**
	 * 가압 스택 GE. 요구 사양:
	 *   - Duration Policy = Has Duration, Duration Magnitude = SetByCaller(Data.Duration)
	 *   - Modifier ★없음★ (태그만 부여한다)
	 *   - Granted Tags = Buff.WaterPump.Pressure
	 *   - Stacking = Aggregate by Target,
	 *                Stack Limit = RampMaxDamageBonus / RampDamageBonusPerStack (기본 10)
	 *   - Stack Duration Refresh = Refresh on Successful Application
	 *   - Stack Expiration        = ★Clear Entire Stack★  (유예 시간이 지나면 전 스택 소멸)
	 *   - ★Deny Overflow Application 을 체크하지 말 것★ — 체크하면 상한 도달 후 적용이 거부되고,
	 *     거부되면 ★지속시간 갱신도 안 돼★ 만렙에서 스택이 통째로 날아간다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ramp")
	TSoftClassPtr<UGameplayEffect> RampStackEffect;

	// 스택 1개당 물대포 피해 증가분. 0.05 = +5%
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ramp", meta = (ClampMin = "0.0"))
	float RampDamageBonusPerStack = 0.05f;

	/**
	 * 피해 증가분 상한. 0.5 = +50%.
	 * ★이 값이 최종 권위다★ — GE 의 Stack Limit 이 더 크더라도 코드가 여기서 자른다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ramp", meta = (ClampMin = "0.0"))
	float RampMaxDamageBonus = 0.50f;

	/**
	 * 이 시간(초) 동안 적을 한 번도 못 맞히면 스택이 전부 사라진다.
	 * ★피해 틱 간격(TickInterval × DamageApplicationInterval, 기본 1초)보다 반드시 커야 한다.★
	 * 같거나 작으면 매 틱 경계에서 스택이 끊긴다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ramp", meta = (ClampMin = "0.1"))
	float RampResetGrace = 2.0f;

	// ========== ② GR.Asc.PumpSupport (급수 노즐) ==========

	/**
	 * 피격된 적에게 거는 감속. ★Plan5(씨앗칩)의 FDRTimedSpeedEffect 를 그대로 재사용한다.★
	 * ⚠️ Duration 의 구조체 기본값이 5.0(씨앗 폭탄용)이므로 ★에셋에서 반드시 낮춰야 한다★ —
	 *    피해 틱 간격보다 살짝 길게(기본 권장 1.2) 잡는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Support")
	FDRTimedSpeedEffect EnemySlow;

	/**
	 * 피격된 적에게 거는 "받는 피해 증가" GE. 요구 사양:
	 *   - Duration Policy = Has Duration, Duration Magnitude = SetByCaller(Data.Duration)
	 *   - Modifier ★없음★ / Granted Tags = Debuff.Vulnerable
	 *   - Stacking = Aggregate by Source, Stack Limit 1, Refresh on Successful Application
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Support")
	TSoftClassPtr<UGameplayEffect> VulnerableEffect;

	/**
	 * Debuff.Vulnerable 을 가진 대상이 받는 피해 증가분. 0.5 = +50%.
	 * ★중첩하지 않는다★ — 몇 명이 걸든 태그가 있으면 ×1.5 하나뿐이다.
	 * (Plan5(물대포칩) D-11)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Support", meta = (ClampMin = "0.0"))
	float VulnerableDamageTakenBonus = 0.50f;

	// 취약 지속시간(초). 적중마다 갱신된다. 피해 틱 간격과 같으면 경계에서 한 프레임 끊긴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Support", meta = (ClampMin = "0.01"))
	float VulnerableDuration = 1.0f;
};

/**
 * ★Plan5(발톱칩)★ 정원로봇 기본 공격(발톱) 칩의 동적 상수.
 *
 * 카탈로그 Modifiers 로 표현할 수 없는 값만 여기 둔다 —
 * "준 피해의 몇 %를 회복하는가", "어떤 GE 로", "체력을 얼마까지 쓸 수 있는가" 같은 것들이다.
 * 정적 수치(피해 ×5, −50%, 범위 +30% …)는 전부 DA_ChipCatalog 에 있다.
 */
USTRUCT(BlueprintType)
struct FDRClawChipTuning
{
	GENERATED_BODY()

	// ========== ② GR.Asc.ClawDrain (수혈 삽날) ==========

	/**
	 * 적에게 준 피해 대비 회복 비율. 1.0 = 준 피해 전액.
	 *
	 * ★기준은 "적 체력이 실제로 깎인 양"★ 이다 —
	 * 엘리트 ×0.7 / 취약 ×1.5 / Phase1 ×0.7 이 전부 끝난 값이고, 오버킬분은 빠진다.
	 * (체력 5 남은 적을 100 으로 때리면 흡혈은 5)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Claw|Drain", meta = (ClampMin = "0.0"))
	float LifestealRatio = 1.0f;

	/**
	 * 아군을 ★실제로 회복시킨 양★ 대비 자기 체력 소모 비율. 1.0 = 회복시킨 만큼 그대로 잃는다.
	 *
	 * ★"회복시킨 양" 이 기준이라 만피 아군을 때리면 체력이 전혀 나가지 않는다.★
	 * 0.5 로 낮추면 "절반 값에 아군을 살리는" 칩이 된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Claw|Drain", meta = (ClampMin = "0.0"))
	float AllyHealHealthCostRatio = 1.0f;

	/**
	 * 회복에 쓰는 GE. Instant + ExecCalc_Heal + SetByCaller(Heal).
	 * 기존 GE_Heal 을 그대로 지정하면 된다 — ★흡혈(자기 회복)과 아군 회복이 같은 GE 를 쓴다.★
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Claw|Drain")
	TSoftClassPtr<UGameplayEffect> HealEffect;

	/**
	 * 자기 체력을 ★비용으로★ 깎는 GE.
	 *   - Duration Policy = Instant
	 *   - Modifier: DRAttributeSet.Health / Add Base / SetByCaller(Data.HealthCost)
	 *   - ★SetByCaller 값은 양수(비용 액수)로 들어온다. GE 가 빼야 한다★
	 *     (Modifier Magnitude 의 Coefficient 를 -1 로 두거나, SetByCaller 를 음수로 받도록 하지 말 것)
	 *
	 * ⚠️ IncomingDamage 를 쓰면 안 된다 — 컨테이너 오버플로·히트리액트·전투 진입·부품 낙하가
	 *    전부 발동한다. 이건 피해가 아니라 비용이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Claw|Drain")
	TSoftClassPtr<UGameplayEffect> SelfCostEffect;

	/**
	 * 아군 회복 후 시전자에게 ★반드시 남겨 둘 최소 체력★.
	 *
	 * ⚠️ ★0 으로 내리지 말 것.★ 체력을 직접 깎는 경로는 사망 판정(Die())을 타지 않는다 —
	 *    0 이 되면 죽지 않고 "체력 0 인 채 살아 있는" 상태가 된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Claw|Drain", meta = (ClampMin = "1.0"))
	float AllyHealMinSelfHealth = 1.0f;
};

/**
 * ★Plan5(동전칩)★ 자판기 기본 공격(동전) 칩의 동적 상수.
 *
 * 카탈로그 Modifiers 로 표현할 수 없는 값만 여기 둔다 — "적중 1회에 얼마를", "어떤 GE 로".
 * 정적 수치(피해 +20% / −30%, 발사 간격 +0.4초, 크기 +30%)는 전부 DA_ChipCatalog 에 있다.
 */
USTRUCT(BlueprintType)
struct FDRCoinChipTuning
{
	GENERATED_BODY()

	// ========== ② VM.Asc.CoinDrain ==========

	/**
	 * 기본 공격(동전·잭팟 캡슐)이 적에게 ★한 번 적중할 때마다★ 회복하는 체력(고정량).
	 *
	 * ★준 피해와 무관하다★ — 발톱 흡혈(Claw.LifestealRatio)처럼 "비율"이 아니라 "액수"다.
	 * 취약·엘리트·광폭화가 피해를 바꿔도 회복량은 그대로다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coin|Drain", meta = (ClampMin = "0.0"))
	float HitHealAmount = 2.0f;

	/**
	 * 회복에 쓰는 GE. Instant + ExecCalc_Heal + SetByCaller(Heal) — 기존 GE_Heal 을 그대로 지정한다.
	 * ★비우면 에러 없이 무동작★ 이다. Claw.HealEffect 와는 별개 필드라 ★따로 한 번 더★ 지정해야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coin|Drain")
	TSoftClassPtr<UGameplayEffect> HealEffect;
};

/**
 * ★Plan5(잭팟칩)★ 잭팟 캡슐 확률표(0~1). 합이 1 이어야 한다.
 * 코드는 Normalized() 로 방어하지만, 합이 틀리면 디자이너 의도와 다른 확률이 조용히 나간다 —
 * DA_ProgressionConfig 편집 시 검사가 알려 준다 (Plan5 D-61).
 */
USTRUCT(BlueprintType)
struct FDRJackpotOdds
{
	GENERATED_BODY()

	FDRJackpotOdds() = default;
	FDRJackpotOdds(float InBronze, float InSilver, float InGold)
		: Bronze(InBronze), Silver(InSilver), Gold(InGold) {}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jackpot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Bronze = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jackpot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Silver = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jackpot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Gold = 0.1f;

	float Sum() const { return Bronze + Silver + Gold; }

	/** 음수를 0 으로 자르고 합이 1 이 되게 나눈다. 합이 0 이면 Fallback 을 그대로 돌려준다. */
	FDRJackpotOdds Normalized(const FDRJackpotOdds& Fallback) const;
};

/**
 * ★Plan5(잭팟칩)★ 자판기 잭팟 칩 3종의 동적 상수.
 *
 * ★칩이 없을 때의 기본 확률·피해는 여기가 아니다★ — GA BP(GA_VendingMachineBasicAttack)의
 * BronzeCapsuleChance / SilverCapsuleChance / …CapsuleDamage 에 있다.
 * 여기 있는 값은 전부 "그 칩을 꼈을 때만" 쓰인다.
 */
USTRUCT(BlueprintType)
struct FDRJackpotChipTuning
{
	GENERATED_BODY()

	// ========== ① VM.Asc.JackpotGold (황금 레버) ==========

	// 이 칩을 끼면 확률표를 ★통째로★ 이것으로 바꾼다. 기본: 브론즈 75 / 실버 0 / 골드 25
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jackpot|GoldRush")
	FDRJackpotOdds GoldRushOdds = FDRJackpotOdds(0.75f, 0.0f, 0.25f);

	// ========== ② VM.Asc.JackpotDouble (쌍둥이 캡슐) ==========

	/**
	 * 골드를 0 으로 만들 때, 기본 골드 확률 중 ★브론즈로 옮길 비율★(나머지는 실버로).
	 * 0.5 = 절반씩 → 기본 50/40/10 이면 55/45/0.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jackpot|Double", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DoubleGoldToBronzeShare = 0.5f;

	/**
	 * 같은 색 캡슐을 한 발 더 쏘기까지의 지연(초).
	 * ★다음 잭팟까지의 최단 시간(MinFireInterval × 11 = 0.55초)보다 짧아야 한다★ — 길면 두 번째 발이 다음 잭팟에 덮인다 (Plan5 T-56)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jackpot|Double", meta = (ClampMin = "0.01"))
	float DoubleShotDelay = 0.15f;

	// ========== ③ VM.Asc.JackpotPity (적립 카드) ==========

	// 이 칩을 끼면 쓰는 ★자기만의 기본 확률★ (단점). 기본: 브론즈 45 / 실버 50 / 골드 5
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jackpot|Pity")
	FDRJackpotOdds PityBaseOdds = FDRJackpotOdds(0.45f, 0.50f, 0.05f);

	/**
	 * 브론즈/실버가 나올 때마다 ★그 색에서 빼서 골드에 더하는 양★ (%p, 절대값). 0.02 = 2%p.
	 * 한 색이 0 에 닿으면 더 빼지 않고, 골드는 실제로 뺀 만큼만 오른다. 골드가 나오면 초기화 (Plan5 D-57)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jackpot|Pity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PityStep = 0.02f;

	// 이 칩을 끼면 골드 캡슐 기본 피해가 이 값이 된다 (단점 — BP 의 GoldCapsuleDamage 150 대신)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jackpot|Pity", meta = (ClampMin = "0.0"))
	float PityGoldDamage = 120.f;
};

/**
 * 돌파 칩(⑥⑦⑧)의 동적 효과 튜닝 상수. (Plan3(공용칩) §17.4.3)
 *
 * 이 수치들은 카탈로그의 FDRUpgradeModifier 로 표현할 수 없다 —
 * "잃은 컨테이너 수"나 "주변 팀원 수"처럼 매 순간 변하는 값에 곱해지기 때문이다.
 * 그렇다고 C++ 에 상수로 박으면 밸런스를 만질 때마다 재컴파일해야 한다.
 */
USTRUCT(BlueprintType)
struct FDRChipTuning
{
	GENERATED_BODY()

	// ========== ⑥ 광폭화 (CM.Asc.Berserk) ==========

	/**
	 * 체력 컨테이너 1칸을 잃을 때마다 주는 피해 배율에 ★더해지는★ 값.
	 *
	 * ⚠️ 컨테이너 4칸 구성에서는 최대 3칸까지만 잃을 수 있다 →
	 *    0.25 면 실질 최댓값이 2.75(275%)라 BerserkMaxMultiplier(3.0)에 도달하지 못한다.
	 *    마지막 칸에서 정확히 300% 를 찍고 싶으면 1/(칸수-1) = 0.3334 로 올린다.
	 *    (Plan3(공용칩) §17.6.5 / 열린 결정 O-10)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Berserk", meta = (ClampMin = "0.0"))
	float BerserkBonusPerLostContainer = 0.25f;

	// 주는 피해 배율 상한 (DamageDealt 스탯 + 컨테이너 보너스의 합에 걸린다)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Berserk", meta = (ClampMin = "1.0"))
	float BerserkMaxMultiplier = 3.0f;

	// ========== ⑦ 시너지 오라 (CM.Asc.Synergy) ==========

	// 팀원 탐지 반경 (3m = 300uu)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Synergy", meta = (ClampMin = "0.0"))
	float SynergyRadius = 300.f;

	// 오라 갱신 주기(초). 짧을수록 반응이 빠르지만 오버랩 쿼리가 잦아진다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Synergy", meta = (ClampMin = "0.05"))
	float SynergyTickInterval = 0.25f;

	// 스택 1개당 주는 피해 증가 (0.1 = +10%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Synergy", meta = (ClampMin = "0.0"))
	float SynergyDamageDealtPerStack = 0.10f;

	// 스택 1개당 받는 피해 감소 (0.1 = -10%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Synergy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SynergyDamageTakenPerStack = 0.10f;

	// 받는 피해 배율의 하한. 스택이 아무리 쌓여도 이 비율 아래로는 내려가지 않는다(무적 방지).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Synergy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SynergyMinDamageTakenRatio = 0.2f;

	/**
	 * 오라가 부여하는 GE. Infinite / 모디파이어 없음 / GrantedTags = Buff.Upgrade.Synergy.
	 * ★Stacking 은 None(기본값)으로 둔다★ — 적용마다 별개 인스턴스가 생겨야
	 * ASC 태그 카운트가 곧 스택 수가 된다. (Plan3(공용칩) §17.7.3)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Synergy")
	TSoftClassPtr<UGameplayEffect> SynergyBuffEffect;

	// ========== ⑧ 피의 계약 (CM.Asc.BloodCost) ==========

	// 물 소모량 대비 체력 소모 비율. 1.0 = 물 소모량의 100% 를 체력으로 낸다.
	// (기존 "물 부족 시 부족분의 50%" 규칙과는 별개 경로다)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BloodCost", meta = (ClampMin = "0.0"))
	float BloodCostHealthRatio = 1.0f;

	// ========== 정원로봇 씨앗 폭탄 칩 (Plan5(씨앗칩)) ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SeedBomb")
	FDRSeedBombChipTuning SeedBomb;

	// ========== 정원로봇 물대포 칩 (Plan5(물대포칩)) ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WaterPump")
	FDRWaterPumpChipTuning WaterPump;

	// ========== 정원로봇 기본 공격 칩 (Plan5(발톱칩)) ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Claw")
	FDRClawChipTuning Claw;

	// ========== 자판기 기본 공격 칩 (Plan5(동전칩)) ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coin")
	FDRCoinChipTuning Coin;

	// ========== 자판기 잭팟 칩 (Plan5(잭팟칩)) ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jackpot")
	FDRJackpotChipTuning Jackpot;
};

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

	/**
	 * 스팀 파트너 사이트에 등록한 Achievement API Name.
	 * 비워두면 ★스팀 미러링 대상이 아니다★ (로컬 전용 업적).
	 *
	 * 파트너 사이트 등록은 출시 후 삭제가 비가역이라 마지막에 몰아서 한다 — 그전까지는
	 * 이 칸을 비워둔 채 게임 안 업적만 자유롭게 늘려도 된다. (Plan.md 8.1)
	 * 영숫자와 '_' 만 사용 (공백/한글 불가).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Achievement|Steam")
	FString SteamApiName;
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

	// ========== 코스메틱 정의 (옷장) ==========

	// GameInstance BP 에 이미 ProgressionConfig 슬롯이 있으므로 새 슬롯을 늘리지 않고 여기에 매단다.
	// (ChipCatalog 와 같은 방식 — UDRGameInstance::GetCosmeticCatalog 가 단일 접근 경로다)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
	TObjectPtr<UDRCosmeticCatalog> CosmeticCatalog;

	// ========== 슬롯 (카테고리 구분 없는 통합 6칸) ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot", meta = (ClampMin = "0"))
	int32 MaxSlots = 6;

	// 슬롯 해금 비용 (index = 해금할 슬롯 번호-1). 비면 DefaultSlotCost * 번호.
	// 순차 해금이므로 단조 증가로 넣는 것을 권장한다. 예: {400, 700, 1100, 1500, 1900, 2400}
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot")
	TArray<int32> SlotCosts;

	// 비용 배열이 비었을 때의 폴백 단가
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot", meta = (ClampMin = "0"))
	int32 DefaultSlotCost = 500;

	// 마지막으로 해금한 슬롯의 환불 허용 여부 (다른 로봇으로 방향을 틀 수 있게 하는 안전망)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot")
	bool bAllowSlotRefund = true;

	// 슬롯 환불 시 돌려받는 비율 (1.0 = 전액)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RefundRatio = 1.0f;

	// ========== 기본 제공 (Plan3(공용칩) §6) ==========

	/**
	 * 새 세이브에서 ★무료로★ 열어 두는 슬롯 수. 기본 장착 칩이 들어갈 자리다.
	 *
	 * 이 칸들은 재화를 쓰지 않았으므로 환불 대상이 아니고(UDRGameInstance::RefundSlot),
	 * 예산 검증(GetSlotUnlockCostForClass)에서도 제외된다.
	 * 슬롯 가격표(SlotCosts)는 여전히 ★칸 번호 기준★ 이라 3번 칸은 SlotCosts[2] 그대로다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot", meta = (ClampMin = "0"))
	int32 InitialFreeSlots = 2;

	/**
	 * 새 세이브에서 앞 칸부터 순서대로 자동 장착할 칩 Id. 모든 로봇에 공통 적용된다.
	 *
	 * ★여기 넣는 칩의 RequiredSlotTier 는 InitialFreeSlots 이하★ 여야 한다.
	 * 아니면 SanitizeClassState() 가 "해금 단계 미달"로 판단해 다음 실행에서 조용히 빼 버린다.
	 * ValidateCurrencyBudget() 이 이 조건을 검사한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot")
	TArray<FName> DefaultEquippedChips;

	// ========== 돌파 칩 동적 효과 튜닝 (Plan3(공용칩) §17.4.3) ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Chip Tuning")
	FDRChipTuning ChipTuning;

	// ========== 재화 획득 (전부 계정당 1회) ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reward", meta = (TitleProperty = "StageId"))
	TArray<FDRStageRewardDef> StageRewards;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reward", meta = (TitleProperty = "AchievementId"))
	TArray<FDRAchievementDef> Achievements;

	// ========== 조회 헬퍼 ==========

	// 로봇 1대의 통합 슬롯 총 칸 수
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetMaxSlotCount() const;

	// SlotNumber(1-base) 슬롯을 해금하는 비용. 범위를 벗어나면 -1.
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetSlotUnlockCost(int32 SlotNumber) const;

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
