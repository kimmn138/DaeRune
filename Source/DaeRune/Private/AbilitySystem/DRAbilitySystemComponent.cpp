// Copyright DaeRune


#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "DaeRune/DRLogChannels.h"

void UDRAbilitySystemComponent::AbilityActorInfoSet()
{
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UDRAbilitySystemComponent::ClientEffectApplied);
	OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &UDRAbilitySystemComponent::OnRemoveGameplayEffectCallback);
}

void UDRAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities)
{
	for (const TSubclassOf<UGameplayAbility> AbilityClass : StartupAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		if (const UDRGameplayAbility* DRAbility = Cast<UDRGameplayAbility>(AbilitySpec.Ability))
		{
			AbilitySpec.DynamicAbilityTags.AddTag(DRAbility->StartupInputTag);
			FGameplayAbilitySpecHandle Handle = GiveAbility(AbilitySpec);

			// InputTag 캐시에 추가
			if (DRAbility->StartupInputTag.IsValid())
			{
				InputTagToAbilityMap.Add(DRAbility->StartupInputTag, Handle);
			}
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
	if (!InputTag.IsValid()) return;
	if (HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed)) return;

	// 캐시를 사용한 O(1) lookup
	FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecByInputTag(InputTag);
	if (AbilitySpec)
	{
		AbilitySpecInputPressed(*AbilitySpec);
		if (AbilitySpec->IsActive())
		{
			UGameplayAbility* PrimaryInstance = AbilitySpec->GetPrimaryInstance();
			if (PrimaryInstance)
			{
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, AbilitySpec->Handle, PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
			}
		}
	}
}

void UDRAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;
	if (HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputHeld)) return;

	// 캐시를 사용한 O(1) lookup
	FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecByInputTag(InputTag);
	if (AbilitySpec)
	{
		AbilitySpecInputPressed(*AbilitySpec);
		if (!AbilitySpec->IsActive())
		{
			TryActivateAbility(AbilitySpec->Handle);
		}
	}
}

void UDRAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;
	if (HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputReleased)) return;

	// 캐시를 사용한 O(1) lookup
	FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecByInputTag(InputTag);
	if (AbilitySpec && AbilitySpec->IsActive())
	{
		AbilitySpecInputReleased(*AbilitySpec);
		UGameplayAbility* PrimaryInstance = AbilitySpec->GetPrimaryInstance();
		if (PrimaryInstance)
		{
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, AbilitySpec->Handle, PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}
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
		// 캐시된 태그 사용 (FGameplayTag::RequestGameplayTag 호출 비용 제거)
		static const FGameplayTag AbilitiesTag = FGameplayTag::RequestGameplayTag(FName("Abilities"));
		for (const FGameplayTag& Tag : AbilitySpec.Ability.Get()->AbilityTags)
		{
			if (Tag.MatchesTag(AbilitiesTag))
			{
				return Tag;
			}
		}
	}
	return FGameplayTag();
}

FGameplayTag UDRAbilitySystemComponent::GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	// 캐시된 태그 사용 (FGameplayTag::RequestGameplayTag 호출 비용 제거)
	static const FGameplayTag InputTagBase = FGameplayTag::RequestGameplayTag(FName("InputTag"));
	for (const FGameplayTag& Tag : AbilitySpec.DynamicAbilityTags)
	{
		if (Tag.MatchesTag(InputTagBase))
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

void UDRAbilitySystemComponent::OnRemoveGameplayEffectCallback_Implementation(const FActiveGameplayEffect& EffectRemoved)
{
	FGameplayTagContainer TagContainer;
	EffectRemoved.Spec.GetAllGrantedTags(TagContainer);
	
	EffectRemovedDelegate.Broadcast(TagContainer);
}

void UDRAbilitySystemComponent::ClientEffectApplied_Implementation(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	FGameplayTagContainer TagContainer;
	EffectSpec.GetAllGrantedTags(TagContainer);

	const bool HasDuration = EffectSpec.Def->DurationPolicy == EGameplayEffectDurationType::HasDuration;
	const bool DisplayStackCount = EffectSpec.Def->StackingType != EGameplayEffectStackingType::None && EffectSpec.Def->StackLimitCount > 1;

	EffectAssetTags.Broadcast(TagContainer, HasDuration, EffectSpec.Duration, DisplayStackCount, EffectSpec.GetStackCount());
}

FGameplayAbilitySpec* UDRAbilitySystemComponent::FindAbilitySpecByInputTag(const FGameplayTag& InputTag)
{
	// 캐시에서 Handle 조회
	if (const FGameplayAbilitySpecHandle* HandlePtr = InputTagToAbilityMap.Find(InputTag))
	{
		return FindAbilitySpecFromHandle(*HandlePtr);
	}

	// 캐시 미스: 폴백으로 전체 검색 (캐시 동기화 문제 대비)
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			// 캐시에 추가
			InputTagToAbilityMap.Add(InputTag, AbilitySpec.Handle);
			return &AbilitySpec;
		}
	}

	return nullptr;
}

void UDRAbilitySystemComponent::RebuildInputTagCache()
{
	InputTagToAbilityMap.Empty();

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		AddToInputTagCache(AbilitySpec);
	}
}

void UDRAbilitySystemComponent::AddToInputTagCache(const FGameplayAbilitySpec& AbilitySpec)
{
	FGameplayTag InputTag = GetInputTagFromSpec(AbilitySpec);
	if (InputTag.IsValid())
	{
		InputTagToAbilityMap.Add(InputTag, AbilitySpec.Handle);
	}
}

void UDRAbilitySystemComponent::RemoveFromInputTagCache(const FGameplayTag& InputTag)
{
	InputTagToAbilityMap.Remove(InputTag);
}
