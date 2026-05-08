// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRGameInstance.generated.h"

class UDRSaveGame;
class UPlayerCharacterClassInfo;

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

	// 플레이어 전용 CharacterClassInfo (서버/클라이언트 모두 접근 가능)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Class Defaults")
	TObjectPtr<UPlayerCharacterClassInfo> PlayerCharacterClassInfo;

	// ========== 맵 전환 시 캐릭터 선택 보존 ==========

	void SavePlayerClassSelection(const FString& PlayerName, EPlayerCharacterClass SelectedClass);
	EPlayerCharacterClass LoadPlayerClassSelection(const FString& PlayerName) const;
	void SaveAllPlayerSelections(UWorld* World);
	void ClearPlayerClassSelections();

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

	// 맵 전환 시 캐릭터 선택 보존용 (GameInstance는 맵 전환에서 절대 파괴되지 않음)
	TMap<FString, EPlayerCharacterClass> PlayerClassSelections;
};
