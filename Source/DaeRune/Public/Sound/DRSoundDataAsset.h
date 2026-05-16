// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRSoundDataAsset.generated.h"

/**
 * 게임 사운드 데이터를 관리하는 DataAsset
 * Primary Asset으로 등록되어 패키징 시 자동으로 쿠킹됨
 */
UCLASS()
class DAERUNE_API UDRSoundDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Primary Asset ID 반환 (AssetManager에서 사용)
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("SoundData"), GetFName());
	}
	// ���� ����
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

	// Ŭ����/����
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

	// 자판기 로봇
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine")
	TObjectPtr<USoundBase> VendingCoinShotSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine")
	TObjectPtr<USoundBase> VendingJackpotPopSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine")
	TObjectPtr<USoundBase> VendingGainSilverSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine")
	TObjectPtr<USoundBase> VendingGainGoldSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine")
	TObjectPtr<USoundBase> VendingSkillUseSound;

	// 독가스 (Phase3)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoisonGas")
	TObjectPtr<USoundBase> PoisonGasWarningSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PoisonGas")
	TObjectPtr<USoundBase> PoisonGasActiveLoopSound;

	// 적(아르마딜로)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Armadillo")
	TObjectPtr<USoundBase> ArmadilloRollLoopSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Armadillo")
	TObjectPtr<USoundBase> ArmadilloImpactSound;

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
