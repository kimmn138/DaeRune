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
    return GEngine ? Cast<UDRGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UDRGameUserSettings::ApplyCustomSettings()
{
    // 여기선 데이터 유효성 검증
    MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
    BGMVolume = FMath::Clamp(BGMVolume, 0.0f, 1.0f);
    SFXVolume = FMath::Clamp(SFXVolume, 0.0f, 1.0f);
    MouseSensitivity = FMath::Clamp(MouseSensitivity, DRSettingsDefaults::MinMouseSensitivity, DRSettingsDefaults::MaxMouseSensitivity);
    Gamma = FMath::Clamp(Gamma, 0.0f, 100.0f);

    if (GEngine)
    {
        GEngine->DisplayGamma = ToDisplayGamma(Gamma);
    }
}

void UDRGameUserSettings::SetToDefaults()
{
    Super::SetToDefaults();

    // 설정 화면의 기본값과 같은 출처 (DRSettingsDefaults)
    MasterVolume = DRSettingsDefaults::VolumeFromPercent(DRSettingsDefaults::MasterVolumePercent);
    BGMVolume = DRSettingsDefaults::VolumeFromPercent(DRSettingsDefaults::MusicVolumePercent);
    SFXVolume = DRSettingsDefaults::VolumeFromPercent(DRSettingsDefaults::SFXVolumePercent);
    MouseSensitivity = DRSettingsDefaults::SensitivityFromPercent(DRSettingsDefaults::CameraSensitivityPercent);
    Gamma = DRSettingsDefaults::GammaPercent;
    PreferredCulture = TEXT("ko");

    // 그래픽도 설정 화면 기본값으로 (엔진 기본은 프레임 무제한 · VSync 꺼짐)
    FrameRateLimit = static_cast<float>(DRSettingsDefaults::FrameRateLimit);
    bUseVSync = DRSettingsDefaults::bVSyncEnabled;
    ScalabilityQuality.SetFromSingleQualityLevel(DRSettingsDefaults::ScalabilityLevel);
}
