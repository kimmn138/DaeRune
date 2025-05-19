// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagContainer.h"
#include "TargetDataFromCamera.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCameraTargetDataSignature, const FGameplayAbilityTargetDataHandle&, DataHandle);

/**
 * 
 */
UCLASS()
class DAERUNE_API UTargetDataFromCamera : public UAbilityTask
{
	GENERATED_BODY()
	
public:
    UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "TargetDataFromCamera", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
    static UTargetDataFromCamera* CreateTargetDataFromCamera(UGameplayAbility* OwningAbility, float TraceDistance);

    UPROPERTY(BlueprintAssignable)
    FCameraTargetDataSignature ValidData;

private:
    float MaxTraceDistance;

    virtual void Activate() override;
    void SendCameraTargetData();

    void OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ActivationTag);
};
