// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "DRProjectileSpell.generated.h"

class ADRProjectile;
class UGameplayEffect;
struct FGameplayTag;

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRProjectileSpell : public UDRDamageGameplayAbility
{
	GENERATED_BODY()
	
public:
	// 업그레이드 칩(SkillProjectileCount)이 반영된 투사체 수. 최소 1발은 보장한다.
	UFUNCTION(BlueprintPure, Category = "Projectile")
	int32 GetEffectiveNumProjectiles() const;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UFUNCTION(BlueprintCallable, Category = "Projectile")
	ADRProjectile* SpawnProjectile(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag, bool bOverridePitch = false, float PitchOverride = 0.f);

	// 카메라 시점 라인트레이스로 조준점(크로스헤어 지점) 계산 — 투사체 계열 GA 공용 유틸
	UFUNCTION(BlueprintPure, Category = "Projectile")
	FVector CalculateTargetLocation() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<ADRProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly)
	int32 NumProjectiles = 5;
};
