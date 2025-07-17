// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "DRProjectileSpell.generated.h"

class ADRProjectile;
class UGameplayEffect;
struct FGameplayTag;

/**
 * 프로젝타일 스펠 어빌리티 클래스 선언
 */
UCLASS()
class DAERUNE_API UDRProjectileSpell : public UDRDamageGameplayAbility
{
	GENERATED_BODY()
	
protected:
	/**
	 * 어빌리티 활성화 시 호출되는 오버라이드 함수 선언
	 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/**
	 * 프로젝타일 생성 함수 선언
	 * @param ProjectileTargetLocation 프로젝타일 목표 위치 벡터
	 * @param SocketTag 소켓 태그
	 * @param bOverridePitch 피치 오버라이드 여부
	 * @param PitchOverride 피치 오버라이드 값
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void SpawnProjectile(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag, bool bOverridePitch = false, float PitchOverride = 0.f);

	/**
	 * 스폰할 프로젝타일 클래스 변수
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<ADRProjectile> ProjectileClass;

	/**
	 * 생성할 프로젝타일 수 변수
	 */
	UPROPERTY(EditDefaultsOnly)
	int32 NumProjectiles = 5;
};
