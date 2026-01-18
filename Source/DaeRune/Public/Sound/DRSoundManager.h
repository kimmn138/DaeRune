// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DRSoundManager.generated.h"

class USoundBase;
class UAudioComponent;
class UDRSoundDataAsset;

/**
 * 霸烙 荤款靛 包府磊
 */
UCLASS()
class DAERUNE_API UDRSoundManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 2D 荤款靛 犁积

	UFUNCTION(BlueprintCallable, Category = "Sound")
	void PlaySound2D(USoundBase* Sound);

	UFUNCTION(BlueprintCallable, Category = "Sound|UI")
	void PlayUIClickSound();

	UFUNCTION(BlueprintCallable, Category = "Sound|Game")
	void PlayPhaseStartSound();

	UFUNCTION(BlueprintCallable, Category = "Sound|Game")
	void PlayWaveStartSound();

	UFUNCTION(BlueprintCallable, Category = "Sound|Game")
	void PlayGameClearSound();

	UFUNCTION(BlueprintCallable, Category = "Sound|Game")
	void PlayGameOverSound();

	// 3D 荤款靛 犁积

	UFUNCTION(BlueprintCallable, Category = "Sound")
	void PlaySoundAtLocation(USoundBase* Sound, const FVector& Location);

	UFUNCTION(BlueprintCallable, Category = "Sound|Actor")
	void PlayPartPickupSound(const FVector& Location);

	UFUNCTION(BlueprintCallable, Category = "Sound|Actor")
	void PlayPartInstallSound(const FVector& Location, bool bIsComplete);

	UFUNCTION(BlueprintCallable, Category = "Sound|Actor")
	void PlaySeedExplosionSound(const FVector& Location);

	UFUNCTION(BlueprintCallable, Category = "Sound|Actor")
	void PlayWaterGainSound(const FVector& Location);

	// Loop 荤款靛 包府
	UFUNCTION(BlueprintCallable, Category = "Sound|Loop")
	UAudioComponent* StartCleanserOperatingSound(const FVector& Location);

	// BGM 包府

	UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
	void PlayBGM(USoundBase* NewBGM, float FadeInDuration = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
	void PlayStageBGM();

	UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
	void PlayLobbyBGM();

	UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
	void PlayMainMenuBGM();

	UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
	void StopBGM(float FadeOutDuration = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
	void CrossfadeBGM(USoundBase* NewBGM, float CrossfadeDuration = 2.0f);

private:
	UPROPERTY()
	TObjectPtr<UAudioComponent> CurrentBGMComponent;

	UPROPERTY()
	TObjectPtr<UAudioComponent> PreviousBGMComponent;

	UAudioComponent* CreateBGMComponent(USoundBase* Sound);

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<UDRSoundDataAsset> SoundData;
};
