// Copyright DaeRune


#include "Game/DRGameUserSettings.h"

UDRGameUserSettings* UDRGameUserSettings::GetDRGameUserSettings()
{
    return Cast<UDRGameUserSettings>(GEngine->GetGameUserSettings());
}

void UDRGameUserSettings::ApplyCustomSettings()
{
    // 여기선 데이터 유효성 검증
    MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
    BGMVolume = FMath::Clamp(BGMVolume, 0.0f, 1.0f);
    SFXVolume = FMath::Clamp(SFXVolume, 0.0f, 1.0f);
    MouseSensitivity = FMath::Clamp(MouseSensitivity, 0.1f, 5.0f);
}

void UDRGameUserSettings::SetToDefaults()
{
    Super::SetToDefaults();

    MasterVolume = 1.0f;
    BGMVolume = 1.0f;
    SFXVolume = 1.0f;
    MouseSensitivity = 1.0f;
}
