// Copyright DaeRune


#include "AbilitySystem/Abilities/DRProjectileSpell.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actor/DRProjectile.h"
#include "Interaction/CombatInterface.h"

/**
 * 어빌리티 활성화 시 호출되는 구현부
 */
void UDRProjectileSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

/**
 * 프로젝타일 생성 구현부
 */
void UDRProjectileSpell::SpawnProjectile(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag, bool bOverridePitch, float PitchOverride)
{
	// 서버 권한 여부 확인
	const bool bIsServer = GetAvatarActorFromActorInfo()->HasAuthority();
	if (!bIsServer) return; // 서버가 아닐 경우 종료

	// 소켓 위치 획득
	const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
		GetAvatarActorFromActorInfo(),
		SocketTag);
	// 목표 위치로부터 회전 계산
	FRotator Rotation = (ProjectileTargetLocation - SocketLocation).Rotation();

	// 피치 오버라이드 적용 여부 분기
	if (bOverridePitch)
	{ 
		Rotation.Pitch = PitchOverride;
	}

	// 스폰 트랜스폼 설정
	FTransform SpawnTransform;
	SpawnTransform.SetLocation(SocketLocation);
	SpawnTransform.SetRotation(Rotation.Quaternion());

	// 스폰 지연 생성 함수 호출
	ADRProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADRProjectile>(
		ProjectileClass,
		SpawnTransform,
		GetOwningActorFromActorInfo(),
		Cast<APawn>(GetOwningActorFromActorInfo()),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	// 데미지 이펙트 파라미터 설정
	Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();

	// 스폰 완료 호출
	Projectile->FinishSpawning(SpawnTransform);
}
