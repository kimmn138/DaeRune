// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "DRAssetManager.generated.h"

/**
 * DaeRune 커스텀 에셋 매니저
 */
UCLASS()
class DAERUNE_API UDRAssetManager : public UAssetManager
{
	GENERATED_BODY()
	
public:
	// 싱글톤 접근자
	static UDRAssetManager& Get();

protected:
	// 게임 시작 시 초기 로딩
	virtual void StartInitialLoading() override;
};
