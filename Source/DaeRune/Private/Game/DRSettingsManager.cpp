// Copyright DaeRune


#include "Game/DRSettingsManager.h"
#include "Game/DRGameUserSettings.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Character/DRCharacter.h"
#include "EngineUtils.h"
#include "Components/AudioComponent.h"

// ���� ��� ����

void UDRSettingsManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (!GameSoundMix)
    {
        GameSoundMix = LoadObject<USoundMix>(nullptr, TEXT("/Game/Blueprints/Audio/SoundMix/SM_GameMix.SM_GameMix"));
    }
    if (!MasterSoundClass)
    {
        MasterSoundClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Blueprints/Audio/SoundClasses/SC_Master.SC_Master"));
    }
    if (!BGMSoundClass)
    {
        BGMSoundClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Blueprints/Audio/SoundClasses/SC_BGM.SC_BGM"));
    }
    if (!SFXSoundClass)
    {
        SFXSoundClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Blueprints/Audio/SoundClasses/SC_SFX.SC_SFX"));
    }
    if (!VoiceSoundClass)
    {
        VoiceSoundClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Blueprints/Audio/SoundClasses/SC_Voice.SC_Voice"));
    }

    // 로드 실패 시 경고 로그
    if (!GameSoundMix)
    {
        UE_LOG(LogTemp, Warning, TEXT("DRSettingsManager: Failed to load GameSoundMix from /Game/Blueprints/Audio/SoundMix/SM_GameMix"));
    }
    if (!MasterSoundClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("DRSettingsManager: Failed to load MasterSoundClass from /Game/Blueprints/Audio/SoundClasses/SC_Master"));
    }
    if (!BGMSoundClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("DRSettingsManager: Failed to load BGMSoundClass from /Game/Blueprints/Audio/SoundClasses/SC_BGM"));
    }
    if (!SFXSoundClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("DRSettingsManager: Failed to load SFXSoundClass from /Game/Blueprints/Audio/SoundClasses/SC_SFX"));
    }
    if (!VoiceSoundClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("DRSettingsManager: Failed to load VoiceSoundClass from /Game/Blueprints/Audio/SoundClasses/SC_Voice"));
    }

    if (UDRGameUserSettings* Settings = GetSettings())
    {
        // 기본 설정 적용
        Settings->ApplySettings(false);

        // 커스텀 설정 적용
        Settings->ApplyCustomSettings();

        // 저장된 Scalability 설정 적용
        ApplyGraphicsQualitySettings();
    }
}

void UDRSettingsManager::Deinitialize()
{
    Super::Deinitialize();
}

UDRGameUserSettings* UDRSettingsManager::GetSettings() const
{
    return UDRGameUserSettings::GetDRGameUserSettings();
}

void UDRSettingsManager::ApplyAndSaveAllSettings()
{
    UDRGameUserSettings* Settings = GetSettings();
    if (!Settings) return;

    // 커스텀 설정 유효성 검사
    Settings->ApplyCustomSettings();

    // 해상도/창모드 적용
    Settings->ApplyResolutionSettings(false);
    Settings->ApplyNonResolutionSettings();
    Settings->ConfirmVideoMode();

    // 그래픽 품질 적용
    ApplyGraphicsQualitySettings();

    // 오디오 적용
    ApplyAudioSettings();

    // 저장
    Settings->SaveSettings();

    // 델리게이트 브로드캐스트
    OnSettingsApplied.Broadcast();
}

void UDRSettingsManager::ApplyResolutionSettings()
{
    if (UDRGameUserSettings* Settings = GetSettings())
    {
        Settings->ApplyResolutionSettings(false);
        Settings->ConfirmVideoMode();
    }
}

void UDRSettingsManager::ApplyNonResolutionSettings()
{
    if (UDRGameUserSettings* Settings = GetSettings())
    {
        Settings->ApplyNonResolutionSettings();
    }
}

void UDRSettingsManager::ApplyAudioSettings()
{
    UWorld* World = nullptr;
    if (GetGameInstance())
    {
        World = GetGameInstance()->GetWorld();
    }

    if (!World)
    {
        if (GEngine)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.World() && Context.WorldType == EWorldType::Game)
                {
                    World = Context.World();
                    break;
                }
            }
        }
    }

    if (World)
    {
        ApplySoundMixToWorld(World);
    }
}

void UDRSettingsManager::ApplySoundMixToWorld(UWorld* World)
{
    if (!World) return;

    UDRGameUserSettings* Settings = GetSettings();
    if (!Settings) return;

    if (!GameSoundMix) return;

    // SoundMix Ȱ��ȭ
    UGameplayStatics::PushSoundMixModifier(World, GameSoundMix);

    // �� Ŭ���� ���� ����
    if (MasterSoundClass)
    {
        UGameplayStatics::SetSoundMixClassOverride(World, GameSoundMix, MasterSoundClass, Settings->MasterVolume, 1.0f, 0.0f, true);
    }
    if (BGMSoundClass)
    {
        UGameplayStatics::SetSoundMixClassOverride(World, GameSoundMix, BGMSoundClass, Settings->BGMVolume, 1.0f, 0.0f, false);
    }
    if (SFXSoundClass)
    {
        UGameplayStatics::SetSoundMixClassOverride(World, GameSoundMix, SFXSoundClass, Settings->SFXVolume, 1.0f, 0.0f, false);
    }
    if (VoiceSoundClass)
    {
        UGameplayStatics::SetSoundMixClassOverride(World, GameSoundMix, VoiceSoundClass, Settings->VoiceVolume, 1.0f, 0.0f, false);
    }
}

void UDRSettingsManager::SetMasterVolume(float NewVolume)
{
    if (UDRGameUserSettings* Settings = GetSettings())
    {
        Settings->MasterVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
    }
}

void UDRSettingsManager::SetBGMVolume(float NewVolume)
{
    if (UDRGameUserSettings* Settings = GetSettings())
    {
        Settings->BGMVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
    }
}

void UDRSettingsManager::SetSFXVolume(float NewVolume)
{
    if (UDRGameUserSettings* Settings = GetSettings())
    {
        Settings->SFXVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
    }
}

void UDRSettingsManager::SetVoiceVolume(float NewVolume)
{
    if (UDRGameUserSettings* Settings = GetSettings())
    {
        Settings->VoiceVolume = FMath::Clamp(NewVolume, 0.0f, 2.0f);
    }
}

void UDRSettingsManager::SetMouseSensitivity(float NewSensitivity)
{
    if (UDRGameUserSettings* Settings = GetSettings())
    {
        Settings->MouseSensitivity = FMath::Clamp(NewSensitivity, 0.1f, 5.0f);
    }
}

void UDRSettingsManager::SetScreenResolution(FIntPoint NewResolution)
{
    if (UDRGameUserSettings* Settings = GetSettings())
    {
        Settings->SetScreenResolution(NewResolution);
    }
}

void UDRSettingsManager::SetWindowMode(EWindowMode::Type NewMode)
{
    if (UDRGameUserSettings* Settings = GetSettings())
    {
        Settings->SetFullscreenMode(NewMode);
    }
}

void UDRSettingsManager::ResetToDefaults()
{
    if (UDRGameUserSettings* Settings = GetSettings())
    {
        Settings->SetToDefaults();
        ApplyAndSaveAllSettings();
    }
}

TArray<FIntPoint> UDRSettingsManager::GetSupportedResolutions() const
{
    TArray<FIntPoint> Resolutions;
    UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);

    // 최소 해상도 필터링
    TArray<FIntPoint> FilteredResolutions;
    for (const FIntPoint& Res : Resolutions)
    {
        if (Res.X >= 1280 && Res.Y >= 720)
        {
            FilteredResolutions.Add(Res);
        }
    }

    return FilteredResolutions;
}

void UDRSettingsManager::SetGraphicsQuality(int32 QualityLevel)
{
    if (UDRGameUserSettings* Settings = GetSettings())
    {
        // 0: Low, 1: Medium, 2: High, 3: Epic, 4: Cinematic
        QualityLevel = FMath::Clamp(QualityLevel, 0, 4);
        Settings->SetOverallScalabilityLevel(QualityLevel);
    }
}

void UDRSettingsManager::ApplyGraphicsQualitySettings()
{
    if (UDRGameUserSettings* Settings = GetSettings())
    {
        // Scalability 설정 적용
        Settings->ApplyNonResolutionSettings();
    }
}
