// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemGlobals.h"
#include "DRAbilitySystemGlobals.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()
	
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;
};
