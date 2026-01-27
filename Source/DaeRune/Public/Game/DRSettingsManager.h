// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DRSettingsManager.generated.h"

class UDRGameUserSettings;
class USoundMix;
class USoundClass;

// ���� ���� �Ϸ� ��������Ʈ
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsApplied);

/**
 * ���� ���� ������
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
    
    // ��� ���� ���� �� ����
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplyAndSaveAllSettings();

    // �ػ�/â��� ����
    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void ApplyResolutionSettings();

    // �ػ� �� ���� ����
    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void ApplyNonResolutionSettings();

    // ����� ������ ����
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

    // Scalability 설정 (0: Low, 1: Medium, 2: High, 3: Epic, 4: Cinematic)
    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void SetGraphicsQuality(int32 QualityLevel);

    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void ApplyGraphicsQualitySettings();

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ResetToDefaults();

    UFUNCTION(BlueprintPure, Category = "Settings")
    TArray<FIntPoint> GetSupportedResolutions() const;

    UPROPERTY(BlueprintAssignable, Category = "Settings")
    FOnSettingsApplied OnSettingsApplied;

private:
    // SoundMix ���� ���� �Լ�
    void ApplySoundMixToWorld(UWorld* World);

    // ���� ���
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
