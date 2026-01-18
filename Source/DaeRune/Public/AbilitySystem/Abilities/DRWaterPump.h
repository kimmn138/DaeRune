// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "DRWaterPump.generated.h"

class UNiagaraComponent;

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRWaterPump : public UDRDamageGameplayAbility
{
	GENERATED_BODY()

public:
    // 1. 무기 소켓에서 에임 방향으로 레이트레이싱
    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    FVector CalculateWaterBeamEndPoint(const FVector& WeaponSocketLocation, bool& bHitObstacle, FHitResult& OutHitResult);

    // 2. 물대포 경로 상의 모든 적 찾기 (BoxOverlap)
    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    AActor* FindClosestTargetInBeam(const FVector& WeaponSocketLocation, const FVector& BeamEndPoint);

    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    void StartWaterPumpLoop();

    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    void StopWaterPumpLoop();

    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    void PerformWaterPumpTick();

    // 현재 타겟 관리
    UFUNCTION(BlueprintPure, Category = "Water Pump")
    AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

    UFUNCTION(BlueprintPure, Category = "Water Pump")
    bool HasValidTarget() const { return CurrentTarget.IsValid(); }

    UFUNCTION(BlueprintPure, Category = "Water Pump")
    int32 GetDamageTickCount() const { return DamageTickCounter; }

    UFUNCTION(BlueprintPure, Category = "Water Pump")
    FVector GetBeamEndPoint() const { return CachedBeamEndPoint; }

    // 블루프린트 이벤트 (헤더에 추가)
    UFUNCTION(BlueprintImplementableEvent, Category = "Water Pump")
    void OnTargetChanged(AActor* OldTarget, AActor* NewTarget);

    UFUNCTION(BlueprintImplementableEvent, Category = "Water Pump")
    void OnDamageTickReached(AActor* Target);

    UFUNCTION(BlueprintImplementableEvent, Category = "Water Pump")
    void OnBeamEndPointUpdated(const FVector& EndPoint);

    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    FGameplayAbilityTargetDataHandle MakeTargetDataHandleFromActors(AActor* TargetActor);

protected:
    // Niagara System Asset
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    TObjectPtr<UNiagaraSystem> WaterCannonEffect;

    // 발사 소켓 이름
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    FName MuzzleSocketName = FName("TestRightHand");

    // 생성된 Niagara Component
    UPROPERTY()
    TObjectPtr<UNiagaraComponent> FirstPersonBeam;

    UPROPERTY()
    TObjectPtr<UNiagaraComponent> ThirdPersonBeam;

    // 타이머
    FTimerHandle BeamUpdateTimer;

    // 함수들
    void StartBeamEffect();
    void UpdateBeamEndpoint();
    void StopBeamEffect();

    // 무기 사정거리
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float WeaponRange = 1000.f;

    // 물대포 폭
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float BeamWidth = 25.f;

    // 물대포 높이
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float BeamHeight = 25.f;

    // Tick 간격 (0.1초)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float TickInterval = 0.1f;

    // 데미지 적용 간격 (1초 = 10 ticks)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    int32 DamageApplicationInterval = 10;

    // 디버그 시각화 활성화
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    bool bShowDebugVisualization = false;

    // 현재 타겟과 이전 타겟 추적
    UPROPERTY(BlueprintReadOnly, Category = "Water Pump")
    TWeakObjectPtr<AActor> CurrentTarget;

    UPROPERTY(BlueprintReadOnly, Category = "Water Pump")
    TWeakObjectPtr<AActor> PreviousTarget;

    // 캐시된 물대포 끝점 (블루프린트에서 이펙트 위치로 사용)
    UPROPERTY(BlueprintReadOnly, Category = "Water Pump")
    FVector CachedBeamEndPoint;

private:
    FTimerHandle WaterPumpTimerHandle;
    
    // 단일 카운터
    int32 DamageTickCounter = 0;

    // 카메라 에임 방향 계산
    bool GetAimDirection(FVector& OutAimStart, FVector& OutAimDirection) const;
};
