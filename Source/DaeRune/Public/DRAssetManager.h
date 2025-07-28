// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "DRAssetManager.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRAssetManager : public UAssetManager
{
	GENERATED_BODY()
	
public:
	static UDRAssetManager& Get();

protected:
	virtual void StartInitialLoading() override;
};
