// Copyright DaeRune


#include "DRAssetManager.h"
#include "AbilitySystemGlobals.h"
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

	// 프로젝트 GameplayTags 네이티브 등록
	FDRGameplayTags::InitializeNativeGameplayTags();

	// GAS 시스템 글로벌 데이터 초기화
	UAbilitySystemGlobals::Get().InitGlobalData();
}
