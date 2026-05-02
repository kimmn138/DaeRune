// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "ActiveGameplayEffectHandle.h"
#include "DREliteRoar.generated.h"

/**
 * Elite Monster Roar - Aura System
 *
 * After montage playback, activates a 15-second aura.
 * Every 0.5s checks radius (1500, ignoring Z axis):
 * - Enemies entering range: Apply buff GE (attack +20%, move speed -10%)
 * - Enemies leaving range: Remove buff GE
 * - On aura end: Remove all remaining buffs
 *
 * Attack increase: Buff.Elite.Roar tag -> ExecCalc_Damage multiplies by 1.2
 * Move speed decrease: GE Modifier -> MoveSpeed * 0.9
 * No effect on players.
 */
UCLASS()
class DAERUNE_API UDREliteRoar : public UDRGameplayAbility
{
	GENERATED_BODY()

public:
	/** Starts the roar aura. Called from Blueprint after montage completes. */
	UFUNCTION(BlueprintCallable, Category = "Elite|Roar")
	void StartRoarAura();

protected:
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** Roar aura radius (Z-axis ignored, XY plane distance) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Elite|Roar")
	float RoarRadius = 1500.f;

	/** Aura duration in seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Elite|Roar")
	float RoarDuration = 15.f;

	/** Range check interval in seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Elite|Roar")
	float AuraTickInterval = 0.5f;

	/** Buff GameplayEffect class (Infinite Duration, MoveSpeed * 0.9, GrantedTag: Buff.Elite.Roar) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Elite|Roar")
	TSubclassOf<UGameplayEffect> RoarBuffEffectClass;

private:
	/** Called every tick: apply buff to enemies entering range, remove from those leaving */
	void TickRoarAura();

	/** Called when aura duration expires: remove all buffs, cleanup, end ability */
	void EndRoarAura();

	/** Cleanup helper: removes all active buffs and clears timers */
	void CleanupAura();

	/** Tracks currently buffed enemies (Actor -> GE Handle for precise removal) */
	TMap<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle> BuffedEnemies;

	/** Repeating timer for range checks */
	FTimerHandle AuraTickTimerHandle;

	/** One-shot timer for aura expiration */
	FTimerHandle AuraDurationTimerHandle;

	/** Whether aura cleanup has already been performed */
	bool bAuraCleanedUp = false;
};
