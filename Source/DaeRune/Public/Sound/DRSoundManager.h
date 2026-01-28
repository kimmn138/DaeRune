// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DRSoundManager.generated.h"

class USoundBase;
class UAudioComponent;
class UDRSoundDataAsset;

/**
 * 효과음 재생 매니저
 * BGM은 레벨에 배치된 DRBGMActor가 담당
 */
UCLASS()
class DAERUNE_API UDRSoundManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 2D 사운드 재생

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

	// 3D 사운드 재생

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

	// Loop 사운드 시작
	UFUNCTION(BlueprintCallable, Category = "Sound|Loop")
	UAudioComponent* StartCleanserOperatingSound(const FVector& Location);

private:
	UPROPERTY()
	TObjectPtr<UDRSoundDataAsset> SoundData;
};
