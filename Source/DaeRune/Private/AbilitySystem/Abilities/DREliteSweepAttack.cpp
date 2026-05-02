// Copyright DaeRune

#include "AbilitySystem/Abilities/DREliteSweepAttack.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystemComponent.h"

void UDREliteSweepAttack::PerformSweepAttack()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return;

	const FVector Origin = AvatarActor->GetActorLocation();
	const FVector Forward = AvatarActor->GetActorForwardVector();
	const float HalfAngleRad = FMath::DegreesToRadians(SweepAngle * 0.5f);
	const float CosHalfAngle = FMath::Cos(HalfAngleRad);

	// 1. Sphere overlap to find all live objects within radius
	TArray<AActor*> OverlappingActors;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(AvatarActor);

	UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(
		AvatarActor,
		OverlappingActors,
		ActorsToIgnore,
		SweepRadius,
		Origin
	);

	// 2. Angle filtering (semicircle detection)
	for (AActor* Target : OverlappingActors)
	{
		// Calculate direction on XY plane (ignore Z)
		FVector ToTarget = Target->GetActorLocation() - Origin;
		ToTarget.Z = 0.f;
		if (!ToTarget.Normalize()) continue;

		FVector ForwardXY = Forward;
		ForwardXY.Z = 0.f;
		if (!ForwardXY.Normalize()) continue;

		const float DotResult = FVector::DotProduct(ForwardXY, ToTarget);

		// DotProduct >= cos(HalfAngle) means within the arc
		if (DotResult >= CosHalfAngle)
		{
			if (UDRAbilitySystemLibrary::IsNotFriend(AvatarActor, Target))
			{
				FDamageEffectParams Params = MakeDamageEffectParamsFromClassDefaults(Target);
				UDRAbilitySystemLibrary::ApplyDamageEffect(Params);
			}
		}
	}
}
