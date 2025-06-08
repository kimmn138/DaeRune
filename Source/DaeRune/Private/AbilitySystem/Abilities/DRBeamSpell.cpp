// Copyright DaeRune


#include "AbilitySystem/Abilities/DRBeamSpell.h"

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
