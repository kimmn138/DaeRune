// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRSoundDataAsset.generated.h"

/**
 * 사운드 에셋 설정용 DataAsset
 */
UCLASS()
class DAERUNE_API UDRSoundDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
	public:
	// 게임 진행
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game")
	TObjectPtr<USoundBase> PhaseStartSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game")
	TObjectPtr<USoundBase> WaveStartSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game")
	TObjectPtr<USoundBase> GameClearSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game")
	TObjectPtr<USoundBase> GameOverSound;

	// UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<USoundBase> UIButtonClickSound;

	// 클렌저/액터
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	TObjectPtr<USoundBase> CleanserOperatingSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	TObjectPtr<USoundBase> PartPickupSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	TObjectPtr<USoundBase> PartInstallSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	TObjectPtr<USoundBase> PartInstallCompleteSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	TObjectPtr<USoundBase> SeedExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Actor")
	TObjectPtr<USoundBase> WaterGainSound;

	// BGM
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
	TObjectPtr<USoundBase> BGM_MainMenu;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
	TObjectPtr<USoundBase> BGM_Lobby;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
	TObjectPtr<USoundBase> BGM_Stage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
	TObjectPtr<USoundBase> BGM_Boss;
};
