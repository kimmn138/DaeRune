// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRProjectileSpell.h"
#include "DRVacuumAirShot.generated.h"

class UAnimMontage;

/** 공기탄 충전 단계별 데이터 ([0] 기본 ~ [3] 강화) */
USTRUCT(BlueprintType)
struct FVacuumShotStage
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Damage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ProjectileSpeed = 1500.f;
};

/**
 * 로봇 청소기 일반 공격: 공기탄 (LMB, Plan3 §6.1)
 * - 자판기 BasicAttack과 동일한 Pressed→GA활성/Released→BP콜백 구조
 * - 홀드 시간에 따라 0~3단계 (ChargeInterval 단위), 짧은 클릭은 0단계 즉시 발사
 * - 최종 단계(강화탄): 적/아군 넉백 + 시전자 반동 (발밑 사격 = 로켓 점프)
 * - 돌진 상태(지속 돌진 태그 또는 이속버프)에서는 수평 반동 제거
 */
UCLASS()
class DAERUNE_API UDRVacuumAirShot : public UDRProjectileSpell
{
	GENERATED_BODY()

public:
	UDRVacuumAirShot();

	// 충전 시작. ActivateAbility BP에서 호출.
	UFUNCTION(BlueprintCallable, Category = "AirShot")
	void StartCharging();

	// 충전 종료 + 발사 + EndAbility. InputReleased BP에서 호출.
	UFUNCTION(BlueprintCallable, Category = "AirShot")
	void ReleaseAndFire();

	// 현재 충전 단계 (0 ~ Stages.Num()-1)
	UFUNCTION(BlueprintPure, Category = "AirShot")
	int32 GetChargeStage() const;

	// 스킬 아이콘 게이지 칸 수 = 충전 단계 수 - 1 (0단계는 빈 게이지, 기본 4단계 → 3칸)
	UFUNCTION(BlueprintPure, Category = "AirShot")
	int32 GetMaxGauge() const { return FMath::Max(0, Stages.Num() - 1); }

	// 충전 단계 변경 시 (BP: UI/SFX — 로컬 연출)
	UFUNCTION(BlueprintImplementableEvent, Category = "AirShot")
	void OnChargeStageChanged(int32 NewStage);

protected:
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// 단계별 데미지/탄속 ([0] 기본, [1], [2], [3] 강화)
	UPROPERTY(EditDefaultsOnly, Category = "AirShot")
	TArray<FVacuumShotStage> Stages;

	// 단계당 충전 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "AirShot")
	float ChargeInterval = 0.5f;

	// 최종 단계 발사 반동 크기
	UPROPERTY(EditDefaultsOnly, Category = "AirShot")
	float RecoilImpulse = 1200.f;

	// 강화탄 넉백 크기 (적/아군 공통)
	UPROPERTY(EditDefaultsOnly, Category = "AirShot")
	float EnhancedKnockbackForce = 1000.f;

	// 발사 소켓 태그 (CombatSocket.Weapon)
	UPROPERTY(EditDefaultsOnly, Category = "AirShot")
	FGameplayTag FireSocketTag;

	// 3단계(강화탄) 발사 시에만 재생하는 몽타주 (§13.5-③, fire-and-forget — 0~2단계는 애니 없음)
	UPROPERTY(EditDefaultsOnly, Category = "AirShot")
	TObjectPtr<UAnimMontage> EnhancedAttackMontage;

private:
	double ChargeStartTime = 0.0;
	bool bCharging = false;
	int32 LastNotifiedStage = -1;

	// ChargeInterval마다 OnChargeStageChanged 발화 (로컬 연출용)
	FTimerHandle ChargeUITimerHandle;

	void TickChargeUI();
	// OnChargeStageChanged + ASC 게이지 노티파이 (값이 바뀔 때만 1회)
	void NotifyGauge(int32 Stage);
	// 서버에서 투사체 스폰 + 최종 단계 반동
	void FireShot(int32 Stage);
	// 돌진 이속버프(Buff.RobotVacuum.DashSpeed) 활성 여부
	bool HasDashBuff() const;
};
