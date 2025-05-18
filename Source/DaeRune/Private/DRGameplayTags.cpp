// Fill out your copyright notice in the Description page of Project Settings.


#include "DRGameplayTags.h"
#include "GameplayTagsManager.h"

FDRGameplayTags FDRGameplayTags::GameplayTags;

void FDRGameplayTags::InitializeNativeGameplayTags()
{
	GameplayTags.Attributes_Secondary_Armor = UGameplayTagsManager::Get().AddNativeGameplayTag(FName("Attributes.Secondary.Armor"), FString("Reduces damage taken, improves Block Chance"));
}
