// Copyright DaeRune


#include "AbilitySystem/AbilityTasks/TargetDataFromCamera.h"
#include "AbilitySystemComponent.h"

UTargetDataFromCamera* UTargetDataFromCamera::CreateTargetDataFromCamera(UGameplayAbility* OwningAbility, float TraceDistance)
{
    UTargetDataFromCamera* Task = NewAbilityTask<UTargetDataFromCamera>(OwningAbility);
    Task->MaxTraceDistance = TraceDistance;
    return Task;
}

void UTargetDataFromCamera::Activate()
{
    const bool bIsLocallyControlled = Ability->GetCurrentActorInfo()->IsLocallyControlled();
    if (bIsLocallyControlled)
    {
        SendCameraTargetData();
    }
    else
    {
        const FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
        const FPredictionKey ActivationPredictionKey = GetActivationPredictionKey();
        AbilitySystemComponent.Get()->AbilityTargetDataSetDelegate(SpecHandle, ActivationPredictionKey).AddUObject(this, &UTargetDataFromCamera::OnTargetDataReplicatedCallback);
        const bool bCalledDelegate = AbilitySystemComponent.Get()->CallReplicatedTargetDataDelegatesIfSet(SpecHandle, ActivationPredictionKey);
        if (!bCalledDelegate)
        {
            SetWaitingOnRemotePlayerData();
        }
    }
}

void UTargetDataFromCamera::SendCameraTargetData()
{
    FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get());

    FVector CamLoc;
    FRotator CamRot;
    APlayerController* PC = Ability->GetCurrentActorInfo()->PlayerController.Get();
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    FVector Start = CamLoc;
    FVector End = Start + CamRot.Vector() * MaxTraceDistance;

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Ability->GetCurrentActorInfo()->AvatarActor.Get());

    bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

    FGameplayAbilityTargetDataHandle DataHandle;
    FGameplayAbilityTargetData_SingleTargetHit* Data = new FGameplayAbilityTargetData_SingleTargetHit();
    Data->HitResult = Hit;
    DataHandle.Add(Data);

    AbilitySystemComponent->ServerSetReplicatedTargetData(
        GetAbilitySpecHandle(),
        GetActivationPredictionKey(),
        DataHandle,
        FGameplayTag(),
        AbilitySystemComponent->ScopedPredictionKey);

    if (ShouldBroadcastAbilityTaskDelegates())
    {
        ValidData.Broadcast(DataHandle);
    }
}

void UTargetDataFromCamera::OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ActivationTag)
{
    AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        ValidData.Broadcast(DataHandle);
    }
}
