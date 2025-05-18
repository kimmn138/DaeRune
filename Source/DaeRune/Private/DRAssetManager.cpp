// Fill out your copyright notice in the Description page of Project Settings.


#include "DRAssetManager.h"
#include "DRGameplayTags.h"

UDRAssetManager& UDRAssetManager::Get()
{
	check(GEngine);

	UDRAssetManager* DRAssetManager = Cast<UDRAssetManager>(GEngine->AssetManager);
	return *DRAssetManager;
}

void UDRAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	FDRGameplayTags::InitializeNativeGameplayTags();
}
