// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DREffectActor.h"
#include "DRWaterSource.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaterSourceUsedSignature, AActor*, User);

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

    // 물 채우기 GameplayEffect
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Source")
    TSubclassOf<UGameplayEffect> WaterFillEffectClass;

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayWaterGainSound();

    // 수원지가 사용(소비)되었을 때 발화 (서버 권한)
    UPROPERTY(BlueprintAssignable, Category = "Water Source")
    FOnWaterSourceUsedSignature OnWaterSourceUsedDelegate;

protected:
    // DREffectActor의 OnOverlap/OnEndOverlap 오버라이드
    virtual void OnOverlap(AActor* TargetActor) override;
    virtual void OnEndOverlap(AActor* TargetActor) override;

    // 블루프린트에서 호출할 수 있는 함수들
    UFUNCTION(BlueprintCallable, Category = "Water Source")
    void FillPlayerWater(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, Category = "Water Source")
    void SetWaterSourceAvailable(bool bNewAvailable);

    UFUNCTION(BlueprintCallable, Category = "Water Source")
    void StartRechargeTimer();

    // 수원지 재충전 완료
    UFUNCTION()
    void OnSourceRecharged();

    UFUNCTION()
    void OnRep_bIsAvailable();

    // 블루프린트에서 구현할 이벤트
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
