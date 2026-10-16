// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "DRBellBoyTaunt.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * ★Plan8(시선집중)★ 벨보이 스킬1 — 시선 집중 (E).
 *
 *  · 5초 도발 모션(상체 슬롯 — 걸을 수 있다). 모션 동안 다른 스킬 불가 + 피격 경직 없음(BP 의 Block 태그, Plan8 D-18)
 *  · 1초 노티파이 → 0.5초 동안 0.1초마다 30m 구를 훑어 새 적을 도발 (Plan8 D-19)
 *  · 도발 = 적에게 GE_BellBoy_Taunted — 걸린 순간부터 5초, 이속 ×1.1, 주는 피해 ×0.8 (Plan8 D-11 / D-12 / D-13)
 *  · ★어빌리티가 끝나도 도발은 남는다★ — 적마다 자기 5초가 끝날 때 풀린다 (사용자 확정)
 *
 * ★Plan5(도발칩)★ ① 약점 노출 — 도발된 적이 받는 피해 ×1.15 · 주는 피해 감소 없음 (GE 스펙 SetByCaller — Plan5 D-129)
 *                 ② 반경 × SkillAreaRadius — 스캔과 외침 큐가 같은 값 (Plan5 D-130)
 *                 ③ 적의 도발 시간 × SkillDuration — 모션 5초는 그대로 (Plan5 D-131)
 */
UCLASS()
class DAERUNE_API UDRBellBoyTaunt : public UDRGameplayAbility
{
	GENERATED_BODY()

public:
	UDRBellBoyTaunt();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 도발 모션 (BB_Taunt 5초, UpperBody 슬롯 — 걸으면서 도발) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt")
	TObjectPtr<UAnimMontage> TauntMontage;

	/** 판정 시작 이벤트 (Event.Montage.BellBoy.Taunt — 몽타주 1.0초) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt")
	FGameplayTag TauntStartEventTag;

	/** 도발 반경(uu). 3000 = 30m */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt", meta = (ClampMin = "0.0"))
	float TauntRadius = 3000.f;

	/** 판정 유지 시간 / 간격 — 0.5초 동안 0.1초마다 (0.0 ~ 0.5 → 6회) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt", meta = (ClampMin = "0.0"))
	float DetectWindow = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt", meta = (ClampMin = "0.02"))
	float DetectInterval = 0.1f;

	/** 적 1마리의 도발 시간 — ★걸린 순간부터★ (Plan8 D-12) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt", meta = (ClampMin = "0.1"))
	float TauntDuration = 5.f;

	/** 도발된 적의 이동 속도 배율 (1.1 = +10%) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt", meta = (ClampMin = "0.0"))
	float TauntedMoveSpeedMultiplier = 1.1f;

	/** 도발된 적이 "주는" 피해 배율 (0.8 = −20%, 맞는 대상 무관 — Plan8 D-11) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt", meta = (ClampMin = "0.0"))
	float TauntedDamageMultiplier = 0.8f;

	/** 적에게 거는 GE — Has Duration(Data.Duration) / MoveSpeed Multiply(Compound)(Data.MoveSpeed.Mult) / Target Tags Debuff.Taunted */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt")
	TSubclassOf<UGameplayEffect> TauntedEffect;

	/** 켜면 벽 너머의 적은 도발하지 않는다 (기본 꺼짐 — 요구 원문 "30m 이내의 모든 적", Plan8 D-19) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt")
	bool bRequireLineOfSight = false;

	/** 판정 시작 때 벨보이 위치에 실행할 연출 큐 (선택 — 외침·충격파). RawMagnitude = 반경 */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt")
	FGameplayTag TauntStartCueTag;

	// ========== ★Plan8(FP)★ 1인칭 ==========

	/** FP 도발 몽타주(FP_Taunt 5초, DefaultSlot). 비우면 FP 재생 없음. ★TauntStartEventTag 노티파이 금지★ (Plan8 T-30) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|FirstPerson")
	TObjectPtr<UAnimMontage> FirstPersonTauntMontage;

	/** FP 몽타주에서 "TP 판정 시작과 같은 순간"을 표시하는 노티파이 태그 (Event.Montage.BellBoy.FPSync). 없으면 길이 비례 (Plan8 D-35) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|FirstPerson")
	FGameplayTag FirstPersonSyncTag;

private:
	UFUNCTION()
	void OnTauntStart(FGameplayEventData Payload);

	UFUNCTION()
	void OnScan(int32 ActionNumber);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageAborted();

	void ApplyTauntTo(AActor* Enemy);
	bool HasLineOfSight(const AActor* From, const AActor* To) const;

	// 이번 시전에 이미 도발한 적 — 한 시전에 적당 1회 (Plan8 D-15)
	TSet<TWeakObjectPtr<AActor>> TauntedThisCast;

	// ★Plan5(도발칩)★ ② 이번 시전의 반경(uu) — OnTauntStart 에서 1회 구해 스캔 · 큐가 같이 쓴다 (Plan5 D-130 · T-149)
	float EffectiveTauntRadius = 0.f;

	TObjectPtr<UAnimMontage> ActiveFirstPersonMontage;   // 로컬 제어일 때만 채워진다 (Plan8 D-33)
};
