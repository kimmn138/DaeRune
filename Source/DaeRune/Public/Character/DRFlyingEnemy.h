// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DREnemy.h"
#include "ActiveGameplayEffectHandle.h"
#include "DRFlyingEnemy.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UAudioComponent;
class USoundBase;

/**
 * 비행 적 기본 클래스
 * - 고정 고도(FixedAltitude)를 유지하며 비행
 * - 지형지물(벽/기둥/천장)과는 정상적으로 충돌
 * - 스킬 사용 후 경직(락다운) 상태 진입
 */
UCLASS()
class DAERUNE_API ADRFlyingEnemy : public ADREnemy
{
	GENERATED_BODY()

public:
	ADRFlyingEnemy();
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ===== 경직(락다운) 시스템 =====
	// 스킬 GA(Blueprint)에서 마지막 투사체 발사 후 호출
	UFUNCTION(BlueprintCallable, Category = "FlyingEnemy|Combat")
	void StartSkillLockdown();

	// 경직 중 여부 (AnimBP / BT에서 폴링)
	UFUNCTION(BlueprintPure, Category = "FlyingEnemy|Combat")
	bool IsLockedDown() const { return bIsLockedDown; }

	/** Combat Interface Override */
	virtual void MulticastHandleDeath_Implementation(const FVector& DeathImpulse) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnMoveSpeedChanged(const FOnAttributeChangeData& Data) override;

	// ===== 비행 루프 사운드 (Plan2.md §5.2 방안 A) =====

	/** 비행 루프 SoundCue. SC_DragonFlyFlight 등 Looping=true + Concurrency 자산 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FlyingEnemy|Sound")
	TObjectPtr<USoundBase> FlightSound;

	// ===== 비행 설정 =====

	// 비행 고정 높이 (World Z). -1이면 BeginPlay 시점의 Z를 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlyingEnemy|Movement")
	float FixedAltitude = -1.f;

	// 비행 이동 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlyingEnemy|Movement")
	float FlyingSpeed = 400.f;

	// 비행 감속
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlyingEnemy|Movement")
	float FlyingDeceleration = 800.f;

	// ===== 경직 파라미터 =====

	// 스킬 사용 후 경직 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FlyingEnemy|Combat")
	float SkillLockdownDuration = 3.f;

	// 경직 중 GE (이동 속도 0, 공격 차단 태그)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FlyingEnemy|Combat")
	TSubclassOf<UGameplayEffect> LockdownEffectClass;

	// ===== 사망 추락 설정 =====

	/** 사망 시 적용할 중력 스케일 (1.0 = 정상 중력) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FlyingEnemy|Death")
	float DeathGravityScale = 1.0f;

	/** 착지 후 액터가 소멸되기까지의 대기 시간 (초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FlyingEnemy|Death")
	float PostLandingLifeSpan = 2.0f;

	/** 착지 시 호출되어 추가 처리 수행 */
	UFUNCTION()
	void OnDeathLanded(const FHitResult& Hit);

private:
	// 높이 고정 유지 (Tick에서 호출)
	void MaintainFixedAltitude();

	// 경직 종료 콜백
	void EndSkillLockdown();

	// 비행 루프 정지 (사망/EndPlay 공용). 중복 호출 안전.
	void StopFlightLoop();

	// 비행 루프 오디오 컴포넌트 (BeginPlay에서 spawn, 사망/EndPlay에서 stop)
	UPROPERTY()
	TObjectPtr<UAudioComponent> FlightLoopComponent;

	// ===== 상태 =====

	// 경직 상태 (복제)
	UPROPERTY(Replicated)
	bool bIsLockedDown = false;

	// 경직 중 적용된 GE 핸들
	FActiveGameplayEffectHandle LockdownEffectHandle;

	// 경직 종료 타이머
	FTimerHandle SkillLockdownTimerHandle;
};
