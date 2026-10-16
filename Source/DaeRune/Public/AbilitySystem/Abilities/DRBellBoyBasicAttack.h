// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRMeleeAttack.h"
#include "DRBellBoyBasicAttack.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * ★Plan8(가방)★ 벨보이 기본 공격 — 여행 가방 휘두르기 (LMB).
 *
 *  · 좌/우 몽타주 교대. 첫 타 선0.4/후0.4, 연속 선0.2/후0.1 — 몽타주 재생 속도를
 *    타격 노티파이 앞·뒤로 따로 준다 (Plan8 D-4, Plan5 D-20 의 확장)
 *  · 타격 시각은 몽타주 노티파이에서 읽는다 (Plan8 D-5) — 노티파이를 옮기면 자동으로 따라온다
 *  · 타격부터 HitWindowDuration 동안 서버가 가방 주변을 여러 번 훑는다 (Plan8 D-8)
 *  · 스윙 시작 때 SkillSwitchLockEffect 를 (선딜 + 0.4초) 걸어 다른 스킬을 막는다 (Plan8 D-7)
 *
 * 부모(UDRMeleeAttack)에서 쓰는 것: AttackRadius · GetUpgradedAttackRadius() (범위 칩).
 * 부모의 GetWindupPlayRate / RestoreMontagePlayRate 는 쓰지 않는다 — 재생 속도는 이 클래스가 관리한다.
 */
UCLASS()
class DAERUNE_API UDRBellBoyBasicAttack : public UDRMeleeAttack
{
	GENERATED_BODY()

public:
	UDRBellBoyBasicAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// ========== 에셋 ==========

	/** 3P 스윙 몽타주. [0] → [1] → … 연속 공격마다 번갈아 쓴다(현재 BP: [0] Left, [1] Right — Plan8 T-32). 전부 UpperBody 슬롯 (Plan8 T-2) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|BasicAttack")
	TArray<TObjectPtr<UAnimMontage>> SwingMontages;

	/** 타격 이벤트 — 몽타주의 AN_MontageEvent 가 보내는 태그 (Event.Montage.BellBoy.BasicAttack) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|BasicAttack")
	FGameplayTag HitEventTag;

	/** 노티파이를 못 찾았을 때만 쓰는 타격 시각(몽타주 초). 평소엔 노티파이 시각을 읽는다 (Plan8 D-5) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|BasicAttack", meta = (ClampMin = "0.01"))
	float FallbackHitTime = 0.4f;

	// ========== 타이밍 (실제 초) ==========

	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing", meta = (ClampMin = "0.01"))
	float FirstWindupTime = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing", meta = (ClampMin = "0.01"))
	float FirstRecoveryTime = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing", meta = (ClampMin = "0.01"))
	float ComboWindupTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing", meta = (ClampMin = "0.01"))
	float ComboRecoveryTime = 0.1f;

	/** 앞 스윙이 끝난 뒤 이 시간 안에 다음 스윙이 시작되면 "연속" (Plan8 D-6). 너무 짧으면 서버·클라가 갈린다 (Plan8 T-5) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing", meta = (ClampMin = "0.0"))
	float ComboResetTime = 0.25f;

	/** 다른 스킬로 넘어갈 때의 후딜(타격 기준 초). 스윙 시작 때 (선딜 + 이 값) 동안 다른 스킬을 막는다 (Plan8 D-7) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing", meta = (ClampMin = "0.0"))
	float SkillSwitchRecoveryTime = 0.4f;

	/** 위 차단용 GE — Has Duration(SetByCaller Data.Duration) + Block Abilities with Tags(시선 집중·모자) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing")
	TSubclassOf<UGameplayEffect> SkillSwitchLockEffect;

	// ========== 판정 ==========

	/** 판정 창 길이(실제 초). 0 이면 타격 순간 1회만 — 정원로봇 발톱과 같다 (Plan8 D-8) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hit", meta = (ClampMin = "0.0"))
	float HitWindowDuration = 0.1f;

	/** 판정 창 안에서 훑는 횟수. 4 면 0 / 0.025 / 0.05 / 0.075 초 */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hit", meta = (ClampMin = "1"))
	int32 HitWindowSamples = 4;

	/** 판정 중심 — CombatSocket.LeftHand → BP_BellBoy 의 LeftHandSocketName(BagSocket) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hit")
	FGameplayTag HitSocketTag;

	/** 켜면 판정 구를 0.5초 그린다 (반경 조정용) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hit")
	bool bDrawDebugHit = false;

	// ========== ★Plan8(FP)★ 1인칭 ==========

	/**
	 * FP 스윙 몽타주 — ★SwingMontages 와 같은 순서·같은 개수★ (Plan8 T-32). 비우면 FP 재생 없음.
	 * DefaultSlot. ★타격 이벤트 태그(HitEventTag) 노티파이를 넣지 말 것★ — 판정이 FP 타이밍으로 바뀐다 (Plan8 T-30)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|FirstPerson")
	TArray<TObjectPtr<UAnimMontage>> FirstPersonSwingMontages;

	/** FP 몽타주에서 "TP 타격과 같은 순간"을 표시하는 노티파이 태그 (Event.Montage.BellBoy.FPSync). 없으면 길이 비례 (Plan8 D-35) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|FirstPerson")
	FGameplayTag FirstPersonSyncTag;

private:
	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnHitSample(int32 SampleIndex);

	UFUNCTION()
	void OnHitWindowFinished(int32 ActionNumber);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageAborted();

	// 서버 = ASC::CurrentMontageSetPlayRate(프록시 복제) / 소유 클라 = 로컬 AnimInstance (Plan8 D-10)
	// FP 몽타주는 FPRate 로 따로 — 로컬 제어일 때만 (Plan8 D-36)
	void SetSwingPlayRate(float NewRate, float FPRate);

	// 몽타주와 판정 창이 ★둘 다★ 끝나야 어빌리티를 끝낸다
	void TryFinishSwing();

	// ── 스윙 사이에 유지 (InstancedPerActor — 서버·소유 클라가 각자 들고 있다) ──
	double LastSwingEndTime = -1000.0;
	int32 NextSwingIndex = 0;

	// ── 이번 스윙 ──
	float PendingRecoveryRate = 1.f;
	float PendingFirstPersonRecoveryRate = 1.f;
	TObjectPtr<UAnimMontage> ActiveFirstPersonMontage;   // 로컬 제어일 때만 채워진다 (Plan8 D-33)
	bool bHitEventReceived = false;
	bool bMontageDone = false;
	bool bHitWindowDone = false;
	TSet<TWeakObjectPtr<AActor>> HitActorsThisSwing;
};
