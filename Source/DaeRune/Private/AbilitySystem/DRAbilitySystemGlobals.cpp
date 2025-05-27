// Copyright DaeRune


#include "AbilitySystem/DRAbilitySystemGlobals.h"
#include "DRAbilityTypes.h"

FGameplayEffectContext* UDRAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FDRGameplayEffectContext();
}
