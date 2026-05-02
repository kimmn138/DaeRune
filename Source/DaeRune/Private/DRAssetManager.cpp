// Copyright DaeRune


#include "DRAssetManager.h"
#include "AbilitySystemGlobals.h"
#include "DRGameplayTags.h"
#include "Sound/DRSoundDataAsset.h"

// Primary Asset Type ?뺤쓽
const FPrimaryAssetType UDRAssetManager::SoundDataAssetType = TEXT("SoundData");

UDRAssetManager& UDRAssetManager::Get()
{
	check(GEngine);

	UDRAssetManager* DRAssetManager = Cast<UDRAssetManager>(GEngine->AssetManager);
	return *DRAssetManager;
}

void UDRAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	// ?ㅼ씠?곕툕 GameplayTags 珥덇린??
	FDRGameplayTags::InitializeNativeGameplayTags();

	// GAS ?쒖뒪??湲濡쒕쾶 ?곗씠??珥덇린??
	UAbilitySystemGlobals::Get().InitGlobalData();

	// ?ъ슫???먯뀑 ?ъ쟾 濡쒕뵫
	LoadSoundAssets();
}

void UDRAssetManager::LoadSoundAssets()
{
	// SoundData Primary Asset 濡쒕뵫 ?쒕룄
	TArray<FPrimaryAssetId> SoundDataIds;
	GetPrimaryAssetIdList(SoundDataAssetType, SoundDataIds);

	if (SoundDataIds.Num() > 0)
	{
// ?숆린 濡쒕뵫 (?쒖옉 ???꾩슂?섎?濡?
		TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAssets(SoundDataIds);
		if (Handle.IsValid())
		{
			Handle->WaitUntilComplete();
// 泥?踰덉㎏ 濡쒕뱶??SoundData ?먯뀑 ???
			LoadedSoundData = Cast<UDRSoundDataAsset>(GetPrimaryAssetObject(SoundDataIds[0]));
			if (LoadedSoundData)
			{
return;
			}
		}
	}

	// Primary Asset ?쒖뒪?쒖뿉??紐?李얠쑝硫?吏곸젒 寃쎈줈濡?濡쒕뵫 (?⑦궎吏?鍮뚮뱶 ?대갚)
static const TCHAR* SoundDataPath = TEXT("/Game/Blueprints/Sound/Data/DA_SoundData.DA_SoundData");
	LoadedSoundData = LoadObject<UDRSoundDataAsset>(nullptr, SoundDataPath);

	if (LoadedSoundData)
	{
}
	else
	{
}
}

