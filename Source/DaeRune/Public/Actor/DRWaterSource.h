// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DREffectActor.h"
#include "DRWaterSource.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 *
 */
UCLASS()
class DAERUNE_API ADRWaterSource : public ADREffectActor
{
	GENERATED_BODY()

public:
    ADRWaterSource();

    // 수원지 사용 가능 시 표시할 나이아가라 에셋 1
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Source|VFX")
    TObjectPtr<UNiagaraSystem> AvailableVFXSystem1;

    // 수원지 사용 가능 시 표시할 나이아가라 에셋 2
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Source|VFX")
    TObjectPtr<UNiagaraSystem> AvailableVFXSystem2;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Water Source Properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Source")
    float RechargeDuration = 30.f;

    UPROPERTY(ReplicatedUsing = OnRep_bIsAvailable, BlueprintReadWrite, Category = "Water Source")
    bool bIsAvailable = true;

    // �� ä��� GameplayEffect
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Source")
    TSubclassOf<UGameplayEffect> WaterFillEffectClass;

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayWaterGainSound();

protected:
    // DREffectActor�� OnOverlap/OnEndOverlap �������̵�
    virtual void OnOverlap(AActor* TargetActor) override;
    virtual void OnEndOverlap(AActor* TargetActor) override;

    // ��������Ʈ���� ȣ���� �� �ִ� �Լ���
    UFUNCTION(BlueprintCallable, Category = "Water Source")
    void FillPlayerWater(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, Category = "Water Source")
    void SetWaterSourceAvailable(bool bNewAvailable);

    UFUNCTION(BlueprintCallable, Category = "Water Source")
    void StartRechargeTimer();

    // ������ ������ �Ϸ�
    UFUNCTION()
    void OnSourceRecharged();

    UFUNCTION()
    void OnRep_bIsAvailable();

    // ��������Ʈ���� ������ �̺�Ʈ
    UFUNCTION(BlueprintImplementableEvent, Category = "Water Source")
    void OnWaterSourceUsed(AActor* User);

    UFUNCTION(BlueprintImplementableEvent, Category = "Water Source")
    void OnWaterSourceRecharged();

    UFUNCTION(BlueprintImplementableEvent, Category = "Water Source")
    void OnAvailabilityChanged(bool bNewAvailable);

    virtual void BeginPlay() override;

    // VFX 활성화/비활성화 헬퍼
    void UpdateAvailabilityVFX(bool bAvailable);

    // 나이아가라 컴포넌트 1
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water Source|VFX")
    TObjectPtr<UNiagaraComponent> AvailableVFXComponent1;

    // 나이아가라 컴포넌트 2
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water Source|VFX")
    TObjectPtr<UNiagaraComponent> AvailableVFXComponent2;

private:
    FTimerHandle RechargeTimerHandle;
};
