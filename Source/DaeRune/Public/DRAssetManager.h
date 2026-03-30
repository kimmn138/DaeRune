// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "DRAssetManager.generated.h"

class UDRSoundDataAsset;

/**
 * DaeRune 커스텀 에셋 매니저
 * - GameplayTags 초기화
 * - GAS 글로벌 데이터 초기화
 * - 사운드 에셋 사전 로딩
 */
UCLASS()
class DAERUNE_API UDRAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	// 싱글톤 액세서
	static UDRAssetManager& Get();

	// Primary Asset Type 정의
	static const FPrimaryAssetType SoundDataAssetType;

	// 로드된 SoundData 에셋 반환
	UFUNCTION(BlueprintCallable, Category = "Sound")
	UDRSoundDataAsset* GetSoundDataAsset() const { return LoadedSoundData; }

protected:
	// 게임 시작 시 초기 로딩
	virtual void StartInitialLoading() override;

private:
	// 사운드 에셋 로딩
	void LoadSoundAssets();

	// 로드된 사운드 데이터 에셋
	UPROPERTY()
	TObjectPtr<UDRSoundDataAsset> LoadedSoundData;
};
