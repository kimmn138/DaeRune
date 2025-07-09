// Copyright DaeRune


#include "AbilitySystem/ModMagCalc/MMC_MaxHealth.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Interaction/CombatInterface.h"

UMMC_MaxHealth::UMMC_MaxHealth()
{
}

float UMMC_MaxHealth::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	int32 PlayerLevel = 1; 
	if (Spec.GetContext().GetSourceObject()->Implements<UCombatInterface>())
	{
		PlayerLevel = ICombatInterface::Execute_GetPlayerLevel(Spec.GetContext().GetSourceObject());
	}

	return 80.f + 10.f * PlayerLevel;
}
