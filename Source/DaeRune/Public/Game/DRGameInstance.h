// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DRGameInstance.generated.h"

class UDRSaveGame;

/**
 *
 */
UCLASS()
class DAERUNE_API UDRGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<class UDRSoundDataAsset> SoundDataAsset;

	// ========== 진행도 저장/로드 ==========

	UFUNCTION(BlueprintCallable, Category = "Save")
	bool HasCompletedTutorial() const;

	UFUNCTION(BlueprintCallable, Category = "Save")
	void SetTutorialCompleted();

	UFUNCTION(BlueprintCallable, Category = "Save")
	void LoadProgress();

	UFUNCTION(BlueprintCallable, Category = "Save")
	void SaveProgress();

private:
	UPROPERTY()
	TObjectPtr<UDRSaveGame> CurrentSaveGame;
};
