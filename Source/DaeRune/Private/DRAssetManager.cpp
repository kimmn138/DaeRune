// Copyright DaeRune


#include "DRAssetManager.h"
#include "DaeRune/DRLogChannels.h"
#include "DRGameplayTags.h"
#include "Sound/DRSoundDataAsset.h"

// Primary Asset Type 정의
const FPrimaryAssetType UDRAssetManager::SoundDataAssetType = TEXT("SoundData");

UDRAssetManager& UDRAssetManager::Get()
{
	check(GEngine);
	return *CastChecked<UDRAssetManager>(GEngine->AssetManager);
}

void UDRAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	// 네이티브 GameplayTags 초기화
	FDRGameplayTags::InitializeNativeGameplayTags();

	// (UAbilitySystemGlobals::InitGlobalData 는 UE5.3+ 에서 엔진이 직접 부른다 — 여기서 다시 부를 필요가 없다)

	// 사운드 에셋 사전 로딩
	LoadSoundAssets();
}

void UDRAssetManager::LoadSoundAssets()
{
	// SoundData Primary Asset 로딩 시도
	TArray<FPrimaryAssetId> SoundDataIds;
	GetPrimaryAssetIdList(SoundDataAssetType, SoundDataIds);

	if (SoundDataIds.Num() > 0)
	{
		// 동기 로딩 (시작 시 필요하므로)
		TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAssets(SoundDataIds);
		if (Handle.IsValid())
		{
			Handle->WaitUntilComplete();

			// 첫 번째 로드된 SoundData 에셋 저장
			LoadedSoundData = Cast<UDRSoundDataAsset>(GetPrimaryAssetObject(SoundDataIds[0]));
			if (LoadedSoundData) return;
		}
	}

	// Primary Asset 시스템에서 못 찾으면 직접 경로로 로딩 (패키징 빌드 폴백)
	static const TCHAR* SoundDataPath = TEXT("/Game/Blueprints/Sound/Data/DA_SoundData.DA_SoundData");
	LoadedSoundData = LoadObject<UDRSoundDataAsset>(nullptr, SoundDataPath);
	if (!LoadedSoundData)
	{
		UE_LOG(LogDR, Warning, TEXT("[AssetManager] SoundData 에셋을 찾지 못했습니다: %s"), SoundDataPath);
	}
}
