// Copyright DaeRune

#include "AbilitySystem/Abilities/DREliteRoar.h"
#include "AbilitySystemComponent.h"
#include "Character/DREnemy.h"
#include "Kismet/GameplayStatics.h"
#include "Interaction/CombatInterface.h"
#include "TimerManager.h"

void UDREliteRoar::StartRoarAura()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor || !AvatarActor->HasAuthority()) return;
	if (!RoarBuffEffectClass) return;

	UWorld* World = AvatarActor->GetWorld();
	if (!World) return;

	bAuraCleanedUp = false;

	// Immediately run first tick (apply buffs to enemies already in range)
	TickRoarAura();

	// Start repeating timer for range checks
	World->GetTimerManager().SetTimer(
		AuraTickTimerHandle,
		this,
		&UDREliteRoar::TickRoarAura,
		AuraTickInterval,
		true
	);

	// Start one-shot timer for aura expiration
	World->GetTimerManager().SetTimer(
		AuraDurationTimerHandle,
		this,
		&UDREliteRoar::EndRoarAura,
		RoarDuration,
		false
	);
}

void UDREliteRoar::TickRoarAura()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor || !AvatarActor->HasAuthority()) return;

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC) return;

	const FVector Origin = AvatarActor->GetActorLocation();

	// --- Step 1: Collect all allied enemies currently within range ---
	TSet<AActor*> EnemiesInRange;

	TArray<AActor*> AllEnemies;
	UGameplayStatics::GetAllActorsOfClass(
		AvatarActor->GetWorld(), ADREnemy::StaticClass(), AllEnemies);

	for (AActor* EnemyActor : AllEnemies)
	{
		if (EnemyActor == AvatarActor) continue;

		if (ICombatInterface::Execute_IsDead(EnemyActor)) continue;

		// XY plane distance (ignore Z)
		FVector ToEnemy = EnemyActor->GetActorLocation() - Origin;
		ToEnemy.Z = 0.f;
		const float DistanceXY = ToEnemy.Size();

		if (DistanceXY <= RoarRadius)
		{
			EnemiesInRange.Add(EnemyActor);
		}
	}

	// --- Step 2: Remove buff from enemies that left range or died ---
	TArray<TWeakObjectPtr<AActor>> EnemiesToRemove;

	for (auto& Pair : BuffedEnemies)
	{
		AActor* BuffedEnemy = Pair.Key.Get();
		bool bShouldRemove = false;

		if (!BuffedEnemy)
		{
			bShouldRemove = true;
		}
		else if (!EnemiesInRange.Contains(BuffedEnemy))
		{
			bShouldRemove = true;
		}

		if (bShouldRemove)
		{
			if (BuffedEnemy)
			{
				if (ADREnemy* Enemy = Cast<ADREnemy>(BuffedEnemy))
				{
					if (UAbilitySystemComponent* TargetASC = Enemy->GetAbilitySystemComponent())
					{
						TargetASC->RemoveActiveGameplayEffect(Pair.Value);
					}
				}
			}
			EnemiesToRemove.Add(Pair.Key);
		}
	}

	for (const TWeakObjectPtr<AActor>& Key : EnemiesToRemove)
	{
		BuffedEnemies.Remove(Key);
	}

	// --- Step 3: Apply buff to newly entered enemies ---
	for (AActor* EnemyInRange : EnemiesInRange)
	{
		TWeakObjectPtr<AActor> WeakEnemy(EnemyInRange);

		if (BuffedEnemies.Contains(WeakEnemy)) continue;

		ADREnemy* Enemy = Cast<ADREnemy>(EnemyInRange);
		if (!Enemy) continue;

		UAbilitySystemComponent* TargetASC = Enemy->GetAbilitySystemComponent();
		if (!TargetASC) continue;

		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
			RoarBuffEffectClass, GetAbilityLevel(), SourceASC->MakeEffectContext());

		if (SpecHandle.IsValid())
		{
			FActiveGameplayEffectHandle GEHandle =
				SourceASC->ApplyGameplayEffectSpecToTarget(
					*SpecHandle.Data.Get(), TargetASC);

			BuffedEnemies.Add(WeakEnemy, GEHandle);
		}
	}
}

void UDREliteRoar::EndRoarAura()
{
	CleanupAura();

	if (CurrentActorInfo && HasAuthority(&CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UDREliteRoar::CleanupAura()
{
	if (bAuraCleanedUp) return;
	bAuraCleanedUp = true;

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (AvatarActor)
	{
		if (UWorld* World = AvatarActor->GetWorld())
		{
			World->GetTimerManager().ClearTimer(AuraTickTimerHandle);
			World->GetTimerManager().ClearTimer(AuraDurationTimerHandle);
		}
	}

	// Remove all remaining buffs
	for (auto& Pair : BuffedEnemies)
	{
		AActor* BuffedEnemy = Pair.Key.Get();
		if (!BuffedEnemy) continue;

		if (ADREnemy* Enemy = Cast<ADREnemy>(BuffedEnemy))
		{
			if (UAbilitySystemComponent* TargetASC = Enemy->GetAbilitySystemComponent())
			{
				TargetASC->RemoveActiveGameplayEffect(Pair.Value);
			}
		}
	}
	BuffedEnemies.Empty();
}

void UDREliteRoar::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	CleanupAura();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
