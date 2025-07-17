// Copyright DaeRune


#include "AbilitySystem/Abilities/DRFireBolt.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Actor/DRProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"

/**
 * 파이어볼 프로젝타일 생성 함수 구현부
 */
void UDRFireBolt::SpawnProjectiles(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag, bool bOverridePitch, float PitchOverride, AActor* HomingTarget)
{
	// 서버 권한 체크 처리
	const bool bIsServer = GetAvatarActorFromActorInfo()->HasAuthority();
	if (!bIsServer) return;

	// 소켓 위치 획득 처리
	const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
		GetAvatarActorFromActorInfo(),
		SocketTag);
	// 목표 위치 기반 회전 계산 처리
	FRotator Rotation = (ProjectileTargetLocation - SocketLocation).Rotation();
	if (bOverridePitch) Rotation.Pitch = PitchOverride;

	// 정방향 벡터 계산 처리
	const FVector Forward = Rotation.Vector();
	
	// 균등 분산 회전 배열 생성 처리
	TArray<FRotator> Rotations = UDRAbilitySystemLibrary::EvenlySpacedRotators(Forward, FVector::UpVector, ProjectileSpread, NumProjectiles);

	// 프로젝타일 반복 생성 처리
	for (const FRotator& Rot : Rotations)
	{
		// 스폰 트랜스폼 설정 처리
		FTransform SpawnTransform; 
		SpawnTransform.SetLocation(SocketLocation);
		SpawnTransform.SetRotation(Rot.Quaternion());

		// 프로젝타일 지연 스폰 처리
		ADRProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADRProjectile>(
			ProjectileClass,
			SpawnTransform,
			GetOwningActorFromActorInfo(),
			Cast<APawn>(GetOwningActorFromActorInfo()),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		// 데미지 이펙트 파라미터 설정 처리
		Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();

		// 호밍 타겟 설정 처리
		if (HomingTarget && HomingTarget->Implements<UCombatInterface>())
		{
			Projectile->ProjectileMovement->HomingTargetComponent = HomingTarget->GetRootComponent();
		}
		else
		{
			// 임시 호밍 타겟 컴포넌트 생성 처리
			Projectile->HomingTargetSceneComponent = NewObject<USceneComponent>(USceneComponent::StaticClass());
			Projectile->HomingTargetSceneComponent->SetWorldLocation(ProjectileTargetLocation);
			Projectile->ProjectileMovement->HomingTargetComponent = Projectile->HomingTargetSceneComponent;
		}
		// 호밍 가속도 및 호밍 활성화 설정 처리
		Projectile->ProjectileMovement->HomingAccelerationMagnitude = FMath::FRandRange(HomingAccelerationMin, HomingAccelerationMax);
		Projectile->ProjectileMovement->bIsHomingProjectile = bLaunchHomingProjectiles;

		// 프로젝타일 스폰 완료 처리
		Projectile->FinishSpawning(SpawnTransform);
	}
}
