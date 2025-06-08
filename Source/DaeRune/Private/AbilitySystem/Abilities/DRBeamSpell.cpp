// Copyright DaeRune


#include "AbilitySystem/Abilities/DRBeamSpell.h"
#include "GameFramework/Character.h"

void UDRBeamSpell::StoreCameraDataInfo(const FHitResult& HitResult)
{
	if (HitResult.bBlockingHit)
	{
		CameraHitLocation = HitResult.ImpactPoint;
		CameraHitActor = HitResult.GetActor();
	}
	else
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}

void UDRBeamSpell::StoreOwnerVariables()
{
	if (CurrentActorInfo)
	{
		OwnerCharacter = Cast<ACharacter>(CurrentActorInfo->AvatarActor);
	}
}
