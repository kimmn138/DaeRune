// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "DRVacuumDash.generated.h"

class ADRRobotVacuumCharacter;
class UAnimMontage;

/**
 * 로봇 청소기 2번 스킬: 돌진 (RMB, Plan3 §8)
 * 역할 분담 (Armadillo 패턴 이식):
 * - GA(이 클래스): 게이지 충전 타이머, 물 소모, 버프 GE 적용/감쇠, 충돌 델리게이트 수신 후 데미지/자해, 지속 돌진 시작/종료 명령
 * - 캐릭터(ADRRobotVacuumCharacter): 충돌 감지(OnDashImpact 브로드캐스트), 지속 돌진 자동 전진 Tick, bSustainedDash 복제/연출
 *
 * 게이지: 0.5초당 1칸 (물 20 소모, 부족 시 충전 일시 정지). 해소 시:
 * - 1~4칸: 스택당 이속 +20% 버프, 1초당 1칸 감쇠, 0이 되면 종료
 * - 5칸(최대): 지속 돌진 — 자동 전진 + 감쇠 없음, 충돌(50 자해) 또는 S 브레이크(무피해)로 종료
 */
UCLASS()
class DAERUNE_API UDRVacuumDash : public UDRDamageGameplayAbility
{
	GENERATED_BODY()

public:
	// 게이지 충전 시작. ActivateAbility BP에서 호출.
	UFUNCTION(BlueprintCallable, Category = "Dash")
	void StartChargingGauge();

	// 게이지 해소(돌진 시작). InputReleased BP에서 호출.
	UFUNCTION(BlueprintCallable, Category = "Dash")
	void ReleaseGauge();

	// S 브레이크 — PC 경유 GameplayEvent(Event.Dash.Brake)를 BP WaitGameplayEvent로 수신 후 호출
	UFUNCTION(BlueprintCallable, Category = "Dash")
	void OnBrakePressed();

	UFUNCTION(BlueprintPure, Category = "Dash")
	int32 GetGauge() const { return Gauge; }

	// 게이지 변경 시 (BP: 서버 인스턴스 UI/SFX — 클라 UI는 ASC 노티파이 경로)
	UFUNCTION(BlueprintImplementableEvent, Category = "Dash")
	void OnGaugeChanged(int32 NewGauge, int32 InMaxGauge);

protected:
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	int32 MaxGauge = 5;

	// 1칸 충전 시간
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float ChargeInterval = 0.5f;

	// 1칸 감쇠 시간 (일반 돌진)
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DecayInterval = 1.0f;

	// 1칸당 물 소모량
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float WaterPerGauge = 20.f;

	// 이속 버프 GE (GE_VacuumDashSpeedBuff — Infinite, MoveSpeed ×(1+0.2×스택), StackLimit 5)
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	TSubclassOf<UGameplayEffect> SpeedBuffEffect;

	// 지속 돌진 충돌 자해 GE (GE_VacuumDashSelfDamage — 50)
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	TSubclassOf<UGameplayEffect> SelfDamageEffect;

	// 게이지 1칸당 물 차감 GE — ExecCalc_WaterCost 실행 GE 지정 (GE_Cost_* 관례).
	// 물 부족 시 부족분 ×0.5를 체력으로 대납, SetByCaller 태그는 Cost.Water
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	TSubclassOf<UGameplayEffect> ChargeCostEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float EnemyDamagePerGauge = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float PlayerDamagePerGauge = 5.f;

	// S 브레이크 급정거 몽타주 (지속 돌진 전용, §13.5-④)
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Animation")
	TObjectPtr<UAnimMontage> DashStopMontage;

	// 돌진 충돌 몽타주 (일반/지속 돌진 충돌 공통, §13.5-⑤)
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Animation")
	TObjectPtr<UAnimMontage> DashCrushMontage;

private:
	int32 Gauge = 0;
	bool bDashing = false;          // ReleaseGauge 이후 (버프 적용 상태)
	bool bImpactBound = false;

	FTimerHandle ChargeTimerHandle;
	FTimerHandle DecayTimerHandle;
	FActiveGameplayEffectHandle SpeedBuffHandle;

	// 캐릭터 OnDashImpact 델리게이트 수신 (서버)
	UFUNCTION()
	void HandleDashImpact(AActor* HitActor, const FHitResult& Hit);

	void TickCharge();
	void TickDecay();
	void ApplyBuffStacks(int32 Stacks);
	void ClearAllBuff();
	// 돌진 종료 공통 처리 (게이지/버프/플래그 정리 후 EndAbility)
	void FinishDash(bool bFromImpact, AActor* HitActor);
	// OnGaugeChanged + ASC 클라 노티파이
	void NotifyGauge();

	ADRRobotVacuumCharacter* GetVacuum() const;
};
