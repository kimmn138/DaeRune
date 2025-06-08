// Copyright DaeRune


#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "DaeRune/DRLogChannels.h"

void UDRAbilitySystemComponent::AbilityActorInfoSet()
{
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UDRAbilitySystemComponent::ClientEffectApplied);
}

void UDRAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities)
{
	for (const TSubclassOf<UGameplayAbility> AbilityClass : StartupAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		if (const UDRGameplayAbility* DRAbility = Cast<UDRGameplayAbility>(AbilitySpec.Ability))
		{
			AbilitySpec.DynamicAbilityTags.AddTag(DRAbility->StartupInputTag);
			GiveAbility(AbilitySpec);
		}
	}
	bStartupAbilitiesGiven = true;
	AbilitiesGivenDelegate.Broadcast();
}

void UDRAbilitySystemComponent::AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupPassiveAbilities)
{
	for (const TSubclassOf<UGameplayAbility> AbilityClass : StartupPassiveAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		GiveAbilityAndActivateOnce(AbilitySpec);
	}
}

void UDRAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	UE_LOG(LogTemp, Log, TEXT("AbilityInputTagPressed called with InputTag: %s"), *InputTag.ToString());
	if (!InputTag.IsValid()) return; 

	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		const FString SpecTags = AbilitySpec.GetDynamicSpecSourceTags().ToStringSimple();
		UE_LOG(LogTemp, Log, TEXT("Checking AbilitySpec Handle=%s, Tags=[%s], IsActive=%s"),
			*AbilitySpec.Handle.ToString(),
			*SpecTags,
			AbilitySpec.IsActive() ? TEXT("true") : TEXT("false"));
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			UE_LOG(LogTemp, Log, TEXT("Matched InputTag on AbilitySpec Handle=%s -> Calling AbilitySpecInputPressed"),
				*AbilitySpec.Handle.ToString());
			AbilitySpecInputPressed(AbilitySpec);
			if (AbilitySpec.IsActive())
			{
				UGameplayAbility* PrimaryInstance = AbilitySpec.GetPrimaryInstance();
				if (PrimaryInstance)
				{
					const auto& PredKey = PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey();
					UE_LOG(LogTemp, Log, TEXT("PrimaryInstance found (%s). PredictionKey: %s"),
						*PrimaryInstance->GetName(),
						*PredKey.ToString());
					InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, AbilitySpec.Handle, PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
					UE_LOG(LogTemp, Log, TEXT("InvokedReplicatedEvent InputPressed for Handle=%s"),
						*AbilitySpec.Handle.ToString());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("No PrimaryInstance for AbilitySpec Handle=%s"),
						*AbilitySpec.Handle.ToString());
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AbilitySpec Handle=%s is not active after InputPressed"),
					*AbilitySpec.Handle.ToString());
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AbilityInputTagPressed finished"));
}

void UDRAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag& InputTag)
{
	UE_LOG(LogTemp, Log, TEXT("AbilityInputTagHeld called with InputTag: %s"), *InputTag.ToString());
	if (!InputTag.IsValid()) return;

	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		const FString SpecTags = AbilitySpec.GetDynamicSpecSourceTags().ToStringSimple();
		UE_LOG(LogTemp, Log, TEXT("Checking AbilitySpec Handle=%s, Tags=[%s], IsActive=%s"),
			*AbilitySpec.Handle.ToString(),
			*SpecTags,
			AbilitySpec.IsActive() ? TEXT("true") : TEXT("false"));
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			UE_LOG(LogTemp, Log, TEXT("Matched InputTag on AbilitySpec Handle=%s -> Calling AbilitySpecInputPressed"),
				*AbilitySpec.Handle.ToString());
			AbilitySpecInputPressed(AbilitySpec);
			UE_LOG(LogTemp, Log, TEXT("AbilitySpecInputPressed called for Handle=%s"), *AbilitySpec.Handle.ToString());
			if (!AbilitySpec.IsActive())
			{
				UE_LOG(LogTemp, Log, TEXT("AbilitySpec Handle=%s is not active, calling TryActivateAbility"), *AbilitySpec.Handle.ToString());
				bool bActivated = TryActivateAbility(AbilitySpec.Handle);
				UE_LOG(LogTemp, Log, TEXT("TryActivateAbility returned %s for Handle=%s"),
					bActivated ? TEXT("true") : TEXT("false"),
					*AbilitySpec.Handle.ToString());
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("AbilitySpec Handle=%s was already active, skipping activation"), *AbilitySpec.Handle.ToString());
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("AbilityInputTagHeld finished"));
}

void UDRAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	UE_LOG(LogTemp, Log, TEXT("AbilityInputTagReleased called with InputTag: %s"), *InputTag.ToString());
	if (!InputTag.IsValid()) return;

	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		const FString SpecTags = AbilitySpec.GetDynamicSpecSourceTags().ToStringSimple();
		UE_LOG(LogTemp, Log, TEXT("Checking AbilitySpec Handle=%s, Tags=[%s], IsActive=%s"),
			*AbilitySpec.Handle.ToString(),
			*SpecTags,
			AbilitySpec.IsActive() ? TEXT("true") : TEXT("false"));
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag) && AbilitySpec.IsActive())
		{
			UE_LOG(LogTemp, Log, TEXT("Matched InputTag on AbilitySpec Handle=%s"), *AbilitySpec.Handle.ToString());
			AbilitySpecInputReleased(AbilitySpec);
			UE_LOG(LogTemp, Log, TEXT("Called AbilitySpecInputReleased for Handle=%s"), *AbilitySpec.Handle.ToString());
			UGameplayAbility* PrimaryInstance = AbilitySpec.GetPrimaryInstance();
			if (PrimaryInstance)
			{
				const auto& PredKey = PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey();
				UE_LOG(LogTemp, Log, TEXT("PrimaryInstance found (%s). PredictionKey: %s"),
					*PrimaryInstance->GetName(),
					*PredKey.ToString());
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, AbilitySpec.Handle, PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
				UE_LOG(LogTemp, Log, TEXT("InvokedReplicatedEvent InputReleased for Handle=%s"), *AbilitySpec.Handle.ToString());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("No PrimaryInstance for AbilitySpec Handle=%s"), *AbilitySpec.Handle.ToString());
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("AbilityInputTagReleased finished"));
}

void UDRAbilitySystemComponent::ForEachAbility(const FForEachAbility& Delegate)
{
	FScopedAbilityListLock ActiveScopeLock(*this); 
		for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
		{
			if (!Delegate.ExecuteIfBound(AbilitySpec))
			{
				UE_LOG(LogDR, Error, TEXT("Failed to execute delegate in %hs"), __FUNCTION__);
			}
		}
}

FGameplayTag UDRAbilitySystemComponent::GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	if (AbilitySpec.Ability)
	{
		for (FGameplayTag Tag : AbilitySpec.Ability.Get()->AbilityTags)
		{
			if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Abilities"))))
			{
				return Tag;
			}
		}
	}
	return FGameplayTag();
}

FGameplayTag UDRAbilitySystemComponent::GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	for (FGameplayTag Tag : AbilitySpec.DynamicAbilityTags)
	{
		if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("InputTag"))))
		{
			return Tag; 
		}
	}
	return FGameplayTag();
}

void UDRAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();

	if (!bStartupAbilitiesGiven)
	{
		bStartupAbilitiesGiven = true;
		AbilitiesGivenDelegate.Broadcast();
	}
}

void UDRAbilitySystemComponent::ClientEffectApplied_Implementation(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	FGameplayTagContainer TagContainer;
	EffectSpec.GetAllAssetTags(TagContainer);

	EffectAssetTags.Broadcast(TagContainer);
}
