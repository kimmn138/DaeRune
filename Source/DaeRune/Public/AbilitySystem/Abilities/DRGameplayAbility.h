// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DRGameplayAbility.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag StartupInputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	FScalableFloat Damage;
};
