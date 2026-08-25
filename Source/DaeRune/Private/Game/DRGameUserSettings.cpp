// Copyright DaeRune


#include "Game/DRGameUserSettings.h"
#include "Engine/Engine.h"

namespace
{
	constexpr float MinDisplayGamma = 0.5f;
	constexpr float MaxDisplayGamma = 5.0f;

	// Keep legacy UI default (80) close to UE default display gamma (~2.2).
	float ToDisplayGamma(float UIGammaPercent)
	{
		constexpr float Slope = 1.7f / 80.0f; // 80 -> +1.7 over 0.5 => 2.2
		return FMath::Clamp(MinDisplayGamma + (UIGammaPercent * Slope), MinDisplayGamma, MaxDisplayGamma);
	}
}

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

    if (GEngine)
    {
        GEngine->DisplayGamma = ToDisplayGamma(Gamma);
    }
}

void UDRGameUserSettings::SetToDefaults()
{
    Super::SetToDefaults();

    MasterVolume = 1.0f;
    BGMVolume = 1.0f;
    SFXVolume = 1.0f;
    MouseSensitivity = 1.0f;
    Gamma = 80.0f;
    PreferredCulture = TEXT("ko");
}
