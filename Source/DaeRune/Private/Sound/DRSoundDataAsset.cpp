// Copyright DaeRune


#include "Sound/DRSoundDataAsset.h"
#include "DRAssetManager.h"
#include "Kismet/GameplayStatics.h"

const UDRSoundDataAsset* UDRSoundDataAsset::Get()
{
	const UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized());
	return AssetManager ? AssetManager->GetSoundDataAsset() : nullptr;
}

USoundBase* UDRSoundDataAsset::Find(FSoundMember Sound)
{
	const UDRSoundDataAsset* Data = Get();
	return Data ? (Data->*Sound).Get() : nullptr;
}

void UDRSoundDataAsset::PlayAtLocation(const UObject* WorldContext, FSoundMember Sound, const FVector& Location)
{
	if (USoundBase* SoundAsset = Find(Sound))
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContext, SoundAsset, Location);
	}
}

void UDRSoundDataAsset::Play2D(const UObject* WorldContext, FSoundMember Sound)
{
	if (USoundBase* SoundAsset = Find(Sound))
	{
		UGameplayStatics::PlaySound2D(WorldContext, SoundAsset);
	}
}
