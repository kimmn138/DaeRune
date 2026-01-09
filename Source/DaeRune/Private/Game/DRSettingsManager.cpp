// Copyright DaeRune


#include "Game/DRSettingsManager.h"
#include "Game/DRGameUserSettings.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

// 에셋 경로 정의
const TCHAR* UDRSettingsManager::SOUND_MIX_PATH = TEXT("/Game/Audio/SM_GameMix.SM_GameMix");
const TCHAR* UDRSettingsManager::SC_MASTER_PATH = TEXT("/Game/Audio/SoundClasses/SC_Master.SC_Master");
const TCHAR* UDRSettingsManager::SC_BGM_PATH = TEXT("/Game/Audio/SoundClasses/SC_BGM.SC_BGM");
const TCHAR* UDRSettingsManager::SC_SFX_PATH = TEXT("/Game/Audio/SoundClasses/SC_SFX.SC_SFX");

void UDRSettingsManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UDRGameUserSettings* Settings = GetSettings())
    {
        // 엔진 기본 설정 적용
        Settings->ApplySettings(false);

        // 커스텀 설정 적용
        Settings->ApplyCustomSettings();
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

    // 커스텀 설정 유효성 검증
    Settings->ApplyCustomSettings();

    // 해상도/창모드 적용
    Settings->ApplyResolutionSettings(false);
    Settings->ApplyNonResolutionSettings();
    Settings->ConfirmVideoMode();

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

    // SoundMix 로드
    USoundMix* GameSoundMix = LoadObject<USoundMix>(nullptr, SOUND_MIX_PATH);
    if (!GameSoundMix) return;

    // SoundClass 로드
    USoundClass* MasterClass = LoadObject<USoundClass>(nullptr, SC_MASTER_PATH);
    USoundClass* BGMClass = LoadObject<USoundClass>(nullptr, SC_BGM_PATH);
    USoundClass* SFXClass = LoadObject<USoundClass>(nullptr, SC_SFX_PATH);

    // SoundMix 활성화
    UGameplayStatics::PushSoundMixModifier(World, GameSoundMix);

    // 각 클래스 볼륨 설정
    if (MasterClass)
    {
        UGameplayStatics::SetSoundMixClassOverride(World, GameSoundMix, MasterClass, Settings->MasterVolume, 1.0f, 0.0f, true);
    }
    if (BGMClass)
    {
        UGameplayStatics::SetSoundMixClassOverride(World, GameSoundMix, BGMClass, Settings->BGMVolume, 1.0f, 0.0f, false);
    }
    if (SFXClass)
    {
        UGameplayStatics::SetSoundMixClassOverride(World, GameSoundMix, SFXClass, Settings->SFXVolume, 1.0f, 0.0f, false);
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
