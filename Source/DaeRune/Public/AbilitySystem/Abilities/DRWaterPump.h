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
    // 1. ���� ���Ͽ��� ���� �������� ����Ʈ���̽�
    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    FVector CalculateWaterBeamEndPoint(const FVector& WeaponSocketLocation, bool& bHitObstacle, FHitResult& OutHitResult);

    // 2. ������ ��� ���� ��� �� ã�� (BoxOverlap)
    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    AActor* FindClosestTargetInBeam(const FVector& WeaponSocketLocation, const FVector& BeamEndPoint);

    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    void StartWaterPumpLoop();

    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    void StopWaterPumpLoop();

    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    void PerformWaterPumpTick();

    // ���� Ÿ�� ����
    UFUNCTION(BlueprintPure, Category = "Water Pump")
    AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

    UFUNCTION(BlueprintPure, Category = "Water Pump")
    bool HasValidTarget() const { return CurrentTarget.IsValid(); }

    UFUNCTION(BlueprintPure, Category = "Water Pump")
    int32 GetDamageTickCount() const { return DamageTickCounter; }

    UFUNCTION(BlueprintPure, Category = "Water Pump")
    FVector GetBeamEndPoint() const { return CachedBeamEndPoint; }

    // ��������Ʈ �̺�Ʈ (����� �߰�)
    UFUNCTION(BlueprintImplementableEvent, Category = "Water Pump")
    void OnTargetChanged(AActor* OldTarget, AActor* NewTarget);

    UFUNCTION(BlueprintImplementableEvent, Category = "Water Pump")
    void OnDamageTickReached(AActor* Target);

    UFUNCTION(BlueprintImplementableEvent, Category = "Water Pump")
    void OnBeamEndPointUpdated(const FVector& EndPoint);

    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    FGameplayAbilityTargetDataHandle MakeTargetDataHandleFromActors(AActor* TargetActor);

protected:
    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

    // Niagara System Asset
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    TObjectPtr<UNiagaraSystem> WaterCannonEffect;

    // 채널링 중 자기 자신에게 적용되는 슬로우 GE (서버에서 적용/제거)
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    TSubclassOf<UGameplayEffect> SlowSelfEffectClass;

    // 슬로우 GE 핸들 (Stop/EndAbility에서 제거하기 위해 보관)
    FActiveGameplayEffectHandle ActiveSlowSelfHandle;

    // �߻� ���� �̸�
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    FName MuzzleSocketName = FName("TestRightHand");

    // 1P Niagara Component (3P는 DRCharacter에서 리플리케이트 상태로 관리)
    UPROPERTY()
    TObjectPtr<UNiagaraComponent> FirstPersonBeam;

    // Ÿ�̸�
    FTimerHandle BeamUpdateTimer;

    // �Լ���
    void StartBeamEffect();
    void UpdateBeamEndpoint();
    void StopBeamEffect();

    // ���� �����Ÿ�
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float WeaponRange = 1000.f;

    // ������ ��
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float BeamWidth = 25.f;

    // ������ ����
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float BeamHeight = 25.f;

    // Tick ���� (0.1��)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float TickInterval = 0.1f;

    // ������ ���� ���� (1�� = 10 ticks)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    int32 DamageApplicationInterval = 10;

    // ����� �ð�ȭ Ȱ��ȭ
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    bool bShowDebugVisualization = false;

    // ���� Ÿ�ٰ� ���� Ÿ�� ����
    UPROPERTY(BlueprintReadOnly, Category = "Water Pump")
    TWeakObjectPtr<AActor> CurrentTarget;

    UPROPERTY(BlueprintReadOnly, Category = "Water Pump")
    TWeakObjectPtr<AActor> PreviousTarget;

    // ĳ�õ� ������ ���� (��������Ʈ���� ����Ʈ ��ġ�� ���)
    UPROPERTY(BlueprintReadOnly, Category = "Water Pump")
    FVector CachedBeamEndPoint;

private:
    FTimerHandle WaterPumpTimerHandle;
    
    // ���� ī����
    int32 DamageTickCounter = 0;

    // ī�޶� ���� ���� ���
    bool GetAimDirection(FVector& OutAimStart, FVector& OutAimDirection) const;
};
