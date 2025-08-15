// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DREffectActor.h"
#include "DRWaterSource.generated.h"

class UBoxComponent;

/**
 * 
 */
UCLASS()
class DAERUNE_API ADRWaterSource : public ADREffectActor
{
	GENERATED_BODY()
	
public:
    ADRWaterSource();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Water Source Properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Source")
    float RechargeDuration = 30.f;

    UPROPERTY(ReplicatedUsing = OnRep_bIsAvailable, BlueprintReadWrite, Category = "Water Source")
    bool bIsAvailable = true;

    // 물 채우기 GameplayEffect
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Source")
    TSubclassOf<UGameplayEffect> WaterFillEffectClass;

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

private:
    FTimerHandle RechargeTimerHandle;
};
