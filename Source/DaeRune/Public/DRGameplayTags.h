// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * DRGameplayTags
 *
 * Singleton containing native Gameplay Tags
 */

struct FDRGameplayTags
{
public:
	static const FDRGameplayTags& Get() { return GameplayTags; }
	static void InitializeNativeGameplayTags();

	FGameplayTag Attributes_Primary_Strength;

	FGameplayTag Attributes_Secondary_MaxHealth;

	FGameplayTag InputTag_LMB;
	FGameplayTag InputTag_RMB;
	FGameplayTag InputTag_1;
	FGameplayTag InputTag_2;
	FGameplayTag InputTag_3;
	FGameplayTag InputTag_4;

	FGameplayTag Damage;

	FGameplayTag Effects_HitReact;

private:
	static FDRGameplayTags GameplayTags;
};
