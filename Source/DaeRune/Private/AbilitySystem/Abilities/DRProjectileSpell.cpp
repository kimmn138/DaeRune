// Copyright DaeRune


#include "AbilitySystem/Abilities/DRProjectileSpell.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actor/DRProjectile.h"
#include "Interaction/CombatInterface.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "DaeRune/DRLogChannels.h"

ADRProjectile* UDRProjectileSpell::SpawnProjectile(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag, bool bOverridePitch, float PitchOverride)
{
	const bool bIsServer = GetAvatarActorFromActorInfo()->HasAuthority();
	if (!bIsServer) return nullptr;

	const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
		GetAvatarActorFromActorInfo(),
		SocketTag);
	return SpawnProjectileAt(SocketLocation, ProjectileTargetLocation, bOverridePitch, PitchOverride);
}

ADRProjectile* UDRProjectileSpell::SpawnProjectileAt(const FVector& SpawnLocation, const FVector& ProjectileTargetLocation, bool bOverridePitch, float PitchOverride)
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority()) return nullptr;

	// 투사체 클래스를 BP 에서 비워 두면 크래시 대신 경고
	if (!ProjectileClass)
	{
		UE_LOG(LogDR, Warning, TEXT("[Projectile] %s: ProjectileClass 가 비어 있어 발사하지 않습니다."), *GetName());
		return nullptr;
	}

	FRotator Rotation = (ProjectileTargetLocation - SpawnLocation).Rotation();

	if (bOverridePitch)
	{ 
		Rotation.Pitch = PitchOverride;
	}

	FTransform SpawnTransform;
	SpawnTransform.SetLocation(SpawnLocation);
	SpawnTransform.SetRotation(Rotation.Quaternion());
	// ★Plan5(동전칩)★ 투사체 크기 칩(SkillProjectileScale). 칩이 없으면 1.0 이라 기존과 완전히 같다 —
	// 이 함수를 쓰는 적 GA(원거리·잠자리)는 칩 캐시가 비어 있어 항상 1.0 이다.
	// SpawnActorDeferred 와 FinishSpawning 에 ★같은 트랜스폼★ 이 넘어가야 한다(아래 코드 그대로).
	SpawnTransform.SetScale3D(FVector(GetUpgradedProjectileScale()));

	ADRProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADRProjectile>(
		ProjectileClass,
		SpawnTransform,
		GetOwningActorFromActorInfo(),
		Cast<APawn>(GetOwningActorFromActorInfo()),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile) return nullptr;

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

float UDRProjectileSpell::GetUpgradedProjectileScale() const
{
	// ★기준값 1.0 에 곱한다★ — 칩이 없으면 정확히 1.0.
	// 0 에 가까워지면 충돌 구가 사라져 아무것도 못 맞히므로 하한을 둔다.
	return FMath::Max(0.1f, GetUpgradedFloat(EDRUpgradeStat::SkillProjectileScale, 1.f));
}
