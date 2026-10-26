// Copyright DaeRune


#include "AbilitySystem/Abilities/DRFireBolt.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Actor/DRProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"

void UDRFireBolt::SpawnProjectiles(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag, bool bOverridePitch, float PitchOverride, AActor* HomingTarget)
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority()) return;

	const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
		GetAvatarActorFromActorInfo(),
		SocketTag);
	FRotator Rotation = (ProjectileTargetLocation - SocketLocation).Rotation();
	if (bOverridePitch) Rotation.Pitch = PitchOverride;

	const FVector Forward = Rotation.Vector();
	
	TArray<FRotator> Rotations = UDRAbilitySystemLibrary::EvenlySpacedRotators(Forward, FVector::UpVector, ProjectileSpread, GetEffectiveNumProjectiles());

	for (const FRotator& Rot : Rotations)
	{
		// 스폰 공통 처리(클래스 확인 · 크기 칩 · 피해 파라미터)는 베이스, 유도 설정만 여기서
		SpawnProjectileWithRotation(SocketLocation, Rot, [&](ADRProjectile& Projectile)
		{
			if (HomingTarget && HomingTarget->Implements<UCombatInterface>())
			{
				Projectile.ProjectileMovement->HomingTargetComponent = HomingTarget->GetRootComponent();
			}
			else
			{
				Projectile.HomingTargetSceneComponent = NewObject<USceneComponent>(USceneComponent::StaticClass());
				Projectile.HomingTargetSceneComponent->SetWorldLocation(ProjectileTargetLocation);
				Projectile.ProjectileMovement->HomingTargetComponent = Projectile.HomingTargetSceneComponent;
			}
			Projectile.ProjectileMovement->HomingAccelerationMagnitude = FMath::FRandRange(HomingAccelerationMin, HomingAccelerationMax);
			Projectile.ProjectileMovement->bIsHomingProjectile = bLaunchHomingProjectiles;
		});
	}
}
