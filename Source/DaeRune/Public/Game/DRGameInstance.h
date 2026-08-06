// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRProgressionTypes.h"
#include "DRGameInstance.generated.h"

class UDRSaveGame;
class UPlayerCharacterClassInfo;
class UDRProgressionConfig;

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

	// 레벨/경험치 곡선 및 지급 규칙 (서버/클라 공통). BP에서 DA_ProgressionConfig 지정.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression")
	TObjectPtr<UDRProgressionConfig> ProgressionConfig;

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
	void ResetTutorialProgress();

	UFUNCTION(BlueprintCallable, Category = "Save")
	void LoadProgress();

	UFUNCTION(BlueprintCallable, Category = "Save")
	void SaveProgress();

	// ========== 캐릭터 진행도 API (클라 권위) ==========

	// 특정 클래스의 진행도(복사본). 세이브에 없으면 기본값 {Level=1, XP=0}.
	UFUNCTION(BlueprintPure, Category = "Progression")
	FDRCharacterProgress GetCharacterProgress(EPlayerCharacterClass CharacterClass) const;

	// 특정 클래스의 현재 레벨. 없으면 1.
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetCharacterLevel(EPlayerCharacterClass CharacterClass) const;

	// 모든 클래스 레벨을 배열로 (index = (int)EPlayerCharacterClass). PlayerController 서버 보고용.
	UFUNCTION(BlueprintPure, Category = "Progression")
	TArray<int32> GetAllClassLevels() const;

	// 스테이지 결과 반영: XP 가산 → 다중 레벨업 판정 → 저장. 반환은 결과 UI용 요약.
	UFUNCTION(BlueprintCallable, Category = "Progression")
	FDRStageProgressResult ApplyStageResult(EPlayerCharacterClass CharacterClass, int32 ClearedPhaseCount, bool bGameClear);

private:
	// LoadProgress 후 누락된 클래스 키를 기본값으로 채우고 SaveVersion 마이그레이션.
	void EnsureProgressInitialized();
	UPROPERTY()
	TObjectPtr<UDRSaveGame> CurrentSaveGame;

	// 맵 전환 시 캐릭터 선택 보존용 (GameInstance는 맵 전환에서 절대 파괴되지 않음)
	TMap<FString, EPlayerCharacterClass> PlayerClassSelections;
};
