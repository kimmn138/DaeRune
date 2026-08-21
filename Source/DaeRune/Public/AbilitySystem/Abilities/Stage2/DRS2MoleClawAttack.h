// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "ScalableFloat.h"
#include "DRS2MoleClawAttack.generated.h"

/**
 * 두더지 보스 기본공격 — 전방 120도 / 사거리 2m 부채꼴 2타 (Plan7 §4.2)
 *
 * 몽타주 1개 안의 AnimNotify 2개가 PerformClawSweep(0) / PerformClawSweep(1) 을 호출한다.
 * 각 타가 독립 오버랩이므로 1타 직후 뒤로 빠지면 2타는 맞지 않는다.
 *
 * ★데미지 축은 "구간(1/2/3차)"이다 (Plan7 §2.5-C). 인원수와는 무관하다.
 *   그래서 상속받은 Damage(= 어빌리티 레벨 = 인원수로 조회됨)를 쓰지 않고,
 *   구간을 직접 커브 레벨로 넘기는 전용 FScalableFloat 두 개를 둔다.
 */
UCLASS()
class DAERUNE_API UDRS2MoleClawAttack : public UDRDamageGameplayAbility
{
	GENERATED_BODY()

public:
	UDRS2MoleClawAttack();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/** 몽타주 AnimNotify 에서 호출. HitIndex: 0 = 1타, 1 = 2타 */
	UFUNCTION(BlueprintCallable, Category = "MoleBoss|Claw")
	void PerformClawSweep(int32 HitIndex);

protected:
	/** 부채꼴 사거리. 수평(XY) 거리로 판정한다. 2m = 200uu */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Claw", meta = (ClampMin = "1.0"))
	float SweepRadius = 200.f;

	/** 부채꼴 전체 각도(도). 120 = 전방 좌우 각 60도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Claw", meta = (ClampMin = "1.0", ClampMax = "360.0"))
	float SweepAngle = 120.f;

	/**
	 * 오버랩 구를 SweepRadius 보다 얼마나 크게 잡을지.
	 * ★두더지는 반매몰이라 캡슐 중심이 지면 높이다. 플레이어 중심(지면+88 내외)과 Z 차이가 커서
	 *   반경 200 구로 잡으면 유효 수평거리가 sqrt(200^2 - 88^2) ~= 179 로 줄어 사양(2m)이 깨진다.
	 *   그래서 구는 넉넉히 잡고 판정은 아래에서 XY 거리로 한다 (UDREliteRoar 와 같은 관례).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Claw", meta = (ClampMin = "0.0"))
	float VerticalTolerance = 400.f;

	/**
	 * 1타 데미지. ★커브 레벨 = 구간 + 1 (1차→1, 2차→2, 3차→3).
	 * ★Value 는 반드시 1 로 둔다 — 결과가 Value * Curve[Level] 이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Claw")
	FScalableFloat Hit1Damage;

	/** 2타 데미지. 커브 레벨 규칙은 Hit1Damage 와 같다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Claw")
	FScalableFloat Hit2Damage;

private:
	float ResolveDamage(int32 HitIndex) const;
};
