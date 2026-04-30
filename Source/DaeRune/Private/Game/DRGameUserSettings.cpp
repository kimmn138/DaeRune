// Copyright DaeRune


#include "Game/DRGameUserSettings.h"

UDRGameUserSettings* UDRGameUserSettings::GetDRGameUserSettings()
{
    return Cast<UDRGameUserSettings>(GEngine->GetGameUserSettings());
}

void UDRGameUserSettings::ApplyCustomSettings()
{
    // ���⼱ ������ ��ȿ�� ����
    MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
    BGMVolume = FMath::Clamp(BGMVolume, 0.0f, 1.0f);
    SFXVolume = FMath::Clamp(SFXVolume, 0.0f, 1.0f);
    MouseSensitivity = FMath::Clamp(MouseSensitivity, 0.1f, 5.0f);
    Gamma = FMath::Clamp(Gamma, 0.0f, 100.0f);
}

void UDRGameUserSettings::SetToDefaults()
{
    Super::SetToDefaults();

    MasterVolume = 1.0f;
    BGMVolume = 1.0f;
    SFXVolume = 1.0f;
    MouseSensitivity = 1.0f;
    Gamma = 80.0f;
}
