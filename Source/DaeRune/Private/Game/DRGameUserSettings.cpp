// Copyright DaeRune


#include "Game/DRGameUserSettings.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Kismet/GameplayStatics.h"
#include "AudioDevice.h"

// 에셋 경로 정의
const TCHAR* UDRGameUserSettings::SOUND_MIX_PATH = TEXT("/Game/Blueprints/Audio/SoundMix/SM_GameMix.SM_GameMix");
const TCHAR* UDRGameUserSettings::SC_MASTER_PATH = TEXT("/Game/Blueprints/Audio/SoundClasses/SC_Master.SC_Master");
const TCHAR* UDRGameUserSettings::SC_BGM_PATH = TEXT("/Game/Blueprints/Audio/SoundClasses/SC_BGM.SC_BGM");
const TCHAR* UDRGameUserSettings::SC_SFX_PATH = TEXT("/Game/Blueprints/Audio/SoundClasses/SC_SFX.SC_SFX");
const TCHAR* UDRGameUserSettings::SC_VOICE_PATH = TEXT("/Game/Blueprints/Audio/SoundClasses/SC_Voice.SC_Voice");


UDRGameUserSettings::UDRGameUserSettings()
{
    // 기본값 설정
    MasterVolume = 1.0f;
    BGMVolume = 1.0f;
    SFXVolume = 1.0f;
    VoiceVolume = 1.0f;
    MouseSensitivity = 1.0f;
}

UDRGameUserSettings* UDRGameUserSettings::GetDRGameUserSettings()
{
    return Cast<UDRGameUserSettings>(GEngine->GetGameUserSettings());
}

void UDRGameUserSettings::SetMasterVolume(float NewVolume)
{
    MasterVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
}

void UDRGameUserSettings::SetBGMVolume(float NewVolume)
{
    BGMVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
}

void UDRGameUserSettings::SetSFXVolume(float NewVolume)
{
    SFXVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
}

void UDRGameUserSettings::SetVoiceVolume(float NewVolume)
{
    VoiceVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
}

void UDRGameUserSettings::SetMouseSensitivity(float NewSensitivity)
{
    MouseSensitivity = FMath::Clamp(NewSensitivity, 0.1f, 5.0f);
}

void UDRGameUserSettings::ApplyAudioSettings()
{
    UWorld* World = nullptr;
    if (GEngine)
    {
        // 현재 활성화된 월드 찾기
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && Context.WorldType == EWorldType::Game)
            {
                World = Context.World();
                break;
            }
        }
    }

    if (!World) return;

    // SoundMix 로드
    USoundMix* GameSoundMix = LoadObject<USoundMix>(nullptr, SOUND_MIX_PATH);
    if (!GameSoundMix) return;

    // SoundClass들 로드
    USoundClass* MasterClass = LoadObject<USoundClass>(nullptr, SC_MASTER_PATH);
    USoundClass* BGMClass = LoadObject<USoundClass>(nullptr, SC_BGM_PATH);
    USoundClass* SFXClass = LoadObject<USoundClass>(nullptr, SC_SFX_PATH);
    USoundClass* VoiceClass = LoadObject<USoundClass>(nullptr, SC_VOICE_PATH);

    // SoundMix를 활성화
    UGameplayStatics::PushSoundMixModifier(World, GameSoundMix);

    // 각 SoundClass의 볼륨 조절
    if (MasterClass)
    {
        UGameplayStatics::SetSoundMixClassOverride(
            World,
            GameSoundMix,
            MasterClass,
            MasterVolume,  // Volume
            1.0f,          // Pitch
            0.0f,          // Fade In Time
            true           // Apply To Children
        );
    }

    if (BGMClass)
    {
        UGameplayStatics::SetSoundMixClassOverride(
            World,
            GameSoundMix,
            BGMClass,
            BGMVolume,
            1.0f,
            0.0f,
            false
        );
    }

    if (SFXClass)
    {
        UGameplayStatics::SetSoundMixClassOverride(
            World,
            GameSoundMix,
            SFXClass,
            SFXVolume,
            1.0f,
            0.0f,
            false
        );
    }

    if (VoiceClass)
    {
        UGameplayStatics::SetSoundMixClassOverride(
            World,
            GameSoundMix,
            VoiceClass,
            VoiceVolume,
            1.0f,
            0.0f,
            false
        );
    }
}

void UDRGameUserSettings::SetToDefaults()
{
    Super::SetToDefaults();

    // 설정 기본값
    MasterVolume = 1.0f;
    BGMVolume = 1.0f;
    SFXVolume = 1.0f;
    VoiceVolume = 1.0f;
    MouseSensitivity = 1.0f;
}
