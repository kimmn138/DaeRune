// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "DRWaterPump.generated.h"

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
    bool TraceFromWeaponToAim(const FVector& WeaponSocketLocation, FHitResult& OutHitResult);

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

    // 블루프린트 이벤트 (헤더에 추가)
    UFUNCTION(BlueprintImplementableEvent, Category = "Water Pump")
    void OnTargetChanged(AActor* OldTarget, AActor* NewTarget);

    UFUNCTION(BlueprintImplementableEvent, Category = "Water Pump")
    void OnDamageTickReached(AActor* Target);

    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    FGameplayAbilityTargetDataHandle MakeTargetDataHandleFromActor(AActor* TargetActor);

protected:
    // 무기 사정거리
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float WeaponRange = 1500.f;

    // Tick 간격 (0.1초)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float TickInterval = 0.1f;

    // 데미지 적용 간격 (1초 = 10 ticks)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    int32 DamageApplicationInterval = 5;

    // 현재 타겟과 이전 타겟 추적
    UPROPERTY(BlueprintReadOnly, Category = "Water Pump")
    TWeakObjectPtr<AActor> CurrentTarget;

    UPROPERTY(BlueprintReadOnly, Category = "Water Pump")
    TWeakObjectPtr<AActor> PreviousTarget;

    // 최근 히트 정보
    UPROPERTY(BlueprintReadOnly, Category = "Water Pump")
    FHitResult LastTraceHitResult;

private:
    FTimerHandle WaterPumpTimerHandle;
    int32 DamageTickCounter = 0;
};
