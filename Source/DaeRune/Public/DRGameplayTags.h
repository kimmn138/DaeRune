// Fill out your copyright notice in the Description page of Project Settings.

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

	FGameplayTag Attributes_Secondary_Armor;

protected:

private:
	static FDRGameplayTags GameplayTags;
};
