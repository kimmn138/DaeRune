// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRProjectileSpell.h"
#include "DRFireBolt.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRFireBolt : public UDRProjectileSpell
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void SpawnProjectiles(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag, bool bOverridePitch, float PitchOverride, AActor* HomingTarget);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
	float ProjectileSpread = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
	int32 MaxNumProjectiles = 5;

	UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
	float HomingAccelerationMin = 1600.f;

	UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
	float HomingAccelerationMax = 3200.f;

	UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
	bool bLaunchHomingProjectiles = true;
};
