// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DRSettingsManager.generated.h"

class UDRGameUserSettings;
class USoundMix;
class USoundClass;

// 설정 적용 완료 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsApplied);

/**
 * 게임 설정 관리자
 */
UCLASS()
class DAERUNE_API UDRSettingsManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
	
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    
    UFUNCTION(BlueprintPure, Category = "Settings")
    UDRGameUserSettings* GetSettings() const;
    
    // 모든 설정 적용 및 저장
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplyAndSaveAllSettings();

    // 해상도/창모드 적용
    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void ApplyResolutionSettings();

    // 해상도 외 설정 적용
    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void ApplyNonResolutionSettings();

    // 오디오 설정만 적용
    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void ApplyAudioSettings();

    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void SetMasterVolume(float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void SetBGMVolume(float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void SetSFXVolume(float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void SetVoiceVolume(float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
    void SetMouseSensitivity(float NewSensitivity);

    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void SetScreenResolution(FIntPoint NewResolution);

    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void SetWindowMode(EWindowMode::Type NewMode);

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ResetToDefaults();

    UFUNCTION(BlueprintPure, Category = "Settings")
    TArray<FIntPoint> GetSupportedResolutions() const;

    UPROPERTY(BlueprintAssignable, Category = "Settings")
    FOnSettingsApplied OnSettingsApplied;

private:
    // SoundMix 적용 내부 함수
    void ApplySoundMixToWorld(UWorld* World);

    // 에셋 경로
    UPROPERTY()
    TObjectPtr<USoundMix> GameSoundMix;

    UPROPERTY()
    TObjectPtr<USoundClass> MasterSoundClass;

    UPROPERTY()
    TObjectPtr<USoundClass> BGMSoundClass;

    UPROPERTY()
    TObjectPtr<USoundClass> SFXSoundClass;

    UPROPERTY()
    TObjectPtr<USoundClass> VoiceSoundClass;
};
