// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "DRGameUserSettings.generated.h"

/**
 * DaeRune 커스텀 설정 저장 클래스
 */
UCLASS()
class DAERUNE_API UDRGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
	
public:
    UDRGameUserSettings();

    UFUNCTION(BlueprintCallable, Category = "Settings")
    static UDRGameUserSettings* GetDRGameUserSettings();

    // ========== 사운드 설정 ==========
    
    // 마스터 볼륨 (0.0 ~ 1.0)
    UPROPERTY(Config)
    float MasterVolume;

    // BGM 볼륨 (0.0 ~ 1.0)
    UPROPERTY(Config)
    float BGMVolume;

    // 효과음 볼륨 (0.0 ~ 1.0)
    UPROPERTY(Config)
    float SFXVolume;

    // 보이스 볼륨 (0.0 ~ 1.0)
    UPROPERTY(Config)
    float VoiceVolume;

    // ========== 조작 설정 ==========

    // 마우스 감도 (0.1 ~ 5.0)
    UPROPERTY(Config)
    float MouseSensitivity;

    UFUNCTION(BlueprintCallable, Category = "Settings|Sound")
    float GetMasterVolume() const { return MasterVolume; }

    UFUNCTION(BlueprintCallable, Category = "Settings|Sound")
    void SetMasterVolume(float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "Settings|Sound")
    float GetBGMVolume() const { return BGMVolume; }

    UFUNCTION(BlueprintCallable, Category = "Settings|Sound")
    void SetBGMVolume(float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "Settings|Sound")
    float GetSFXVolume() const { return SFXVolume; }

    UFUNCTION(BlueprintCallable, Category = "Settings|Sound")
    void SetSFXVolume(float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "Settings|Sound")
    float GetVoiceVolume() const { return VoiceVolume; }

    UFUNCTION(BlueprintCallable, Category = "Settings|Sound")
    void SetVoiceVolume(float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
    float GetMouseSensitivity() const { return MouseSensitivity; }

    UFUNCTION(BlueprintCallable, Category = "Settings|Controls")
    void SetMouseSensitivity(float NewSensitivity);

    // ========== 설정 적용 ==========
    
    // 모든 설정을 실제 게임에 적용
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplyAudioSettings();

    // 설정을 기본값으로 초기화
    virtual void SetToDefaults() override;

private:
    // SoundMix 에셋 경로
    static const TCHAR* SOUND_MIX_PATH;

    // SoundClass 에셋 경로들
    static const TCHAR* SC_MASTER_PATH;
    static const TCHAR* SC_BGM_PATH;
    static const TCHAR* SC_SFX_PATH;
    static const TCHAR* SC_VOICE_PATH;
};
