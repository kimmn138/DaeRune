// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DRGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<class UDRSoundDataAsset> SoundDataAsset;
};
