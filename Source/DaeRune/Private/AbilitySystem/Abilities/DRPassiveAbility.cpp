// Copyright DaeRune


#include "AbilitySystem/Abilities/DRPassiveAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"

void UDRPassiveAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetAvatarActorFromActorInfo())))
	{
		DRASC->DeactivatePassiveAbility.AddUObject(this, &UDRPassiveAbility::ReceiveDeactivate);
	}
}

void UDRPassiveAbility::ReceiveDeactivate(const FGameplayTag& AbilityTag)
{
	if (AbilityTags.HasTagExact(AbilityTag))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}
