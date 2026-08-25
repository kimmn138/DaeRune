// Copyright DaeRune


#include "AbilitySystem/Abilities/DRProjectileSpell.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actor/DRProjectile.h"
#include "Interaction/CombatInterface.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

void UDRProjectileSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

ADRProjectile* UDRProjectileSpell::SpawnProjectile(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag, bool bOverridePitch, float PitchOverride)
{
	const bool bIsServer = GetAvatarActorFromActorInfo()->HasAuthority();
	if (!bIsServer) return nullptr;

	const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
		GetAvatarActorFromActorInfo(),
		SocketTag);
	FRotator Rotation = (ProjectileTargetLocation - SocketLocation).Rotation();

	if (bOverridePitch)
	{ 
		Rotation.Pitch = PitchOverride;
	}

	FTransform SpawnTransform;
	SpawnTransform.SetLocation(SocketLocation);
	SpawnTransform.SetRotation(Rotation.Quaternion());

	ADRProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADRProjectile>(
		ProjectileClass,
		SpawnTransform,
		GetOwningActorFromActorInfo(),
		Cast<APawn>(GetOwningActorFromActorInfo()),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();

	Projectile->FinishSpawning(SpawnTransform);

	return Projectile;
}

FVector UDRProjectileSpell::CalculateTargetLocation() const
{
	// 자판기 BasicAttack에서 승격한 공용 조준점 계산 (카메라 라인트레이스)
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return FVector::ZeroVector;

	const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	if (!AvatarPawn) return AvatarActor->GetActorLocation() + AvatarActor->GetActorForwardVector() * 5000.f;

	const APlayerController* PC = Cast<APlayerController>(AvatarPawn->GetController());
	if (!PC) return AvatarActor->GetActorLocation() + AvatarActor->GetActorForwardVector() * 5000.f;

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * 10000.f;

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(AvatarActor);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, CameraLocation, TraceEnd, ECC_Visibility, Params))
	{
		return HitResult.ImpactPoint;
	}

	return TraceEnd;
}

int32 UDRProjectileSpell::GetEffectiveNumProjectiles() const
{
	return GetUpgradedInt(EDRUpgradeStat::SkillProjectileCount, NumProjectiles, 1);
}
