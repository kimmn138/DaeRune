// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "DREliteSweepAttack.generated.h"

/**
 * Elite Monster Sweep Attack
 * Performs a semicircular (180 degree) frontal attack, damaging all players within the arc.
 * Uses Sphere Overlap + Dot Product angle filtering for semicircle detection.
 */
UCLASS()
class DAERUNE_API UDREliteSweepAttack : public UDRDamageGameplayAbility
{
	GENERATED_BODY()

public:
	/** Detects targets within the semicircular area and applies damage. Called from Blueprint at montage notify. */
	UFUNCTION(BlueprintCallable, Category = "Elite|Sweep")
	void PerformSweepAttack();

protected:
	/** Sweep radius from elite's position */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Elite|Sweep")
	float SweepRadius = 300.f;

	/** Sweep angle (total degrees, 180 = semicircle, 360 = full circle) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Elite|Sweep")
	float SweepAngle = 180.f;
};
