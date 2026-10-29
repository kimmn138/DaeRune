// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "DRGameUserSettings.generated.h"

/**
 * 설정 기본값 · 화면 값(%) ↔ 저장 값 변환 — ★단일 출처★.
 * 설정 화면 정의(UDRSettingsManager::BuildDefinitions)와 UDRGameUserSettings::SetToDefaults 가 함께 쓴다.
 * (예전에는 둘이 따로 놀아 첫 실행 화면 값과 "탭 초기화" 값이 달랐다 — 초기화하면 감도가 4배로 뛰었다)
 */
namespace DRSettingsDefaults
{
	// 화면에 보이는 값 (%)
	inline constexpr float MasterVolumePercent = 100.f;
	inline constexpr float MusicVolumePercent = 80.f;
	inline constexpr float SFXVolumePercent = 80.f;
	inline constexpr float CameraSensitivityPercent = 18.f;   // 저장 값 ≈ 0.98 (예전 첫 실행 기본 1.0 과 거의 같다)
	inline constexpr float GammaPercent = 80.f;

	inline constexpr int32 FrameRateLimit = 120;               // 설정 화면 선택지 [30, 60, 120, 무제한(0)]
	inline constexpr bool bVSyncEnabled = true;
	inline constexpr int32 ScalabilityLevel = 3;               // 0 낮음 · 1 보통 · 2 높음 · 3 에픽 · 4 최상

	// 카메라 감도 저장 값 범위
	inline constexpr float MinMouseSensitivity = 0.1f;
	inline constexpr float MaxMouseSensitivity = 5.0f;

	inline float SensitivityFromPercent(float Percent)
	{
		return FMath::Lerp(MinMouseSensitivity, MaxMouseSensitivity, FMath::Clamp(Percent / 100.f, 0.f, 1.f));
	}

	inline float PercentFromSensitivity(float Sensitivity)
	{
		const float Clamped = FMath::Clamp(Sensitivity, MinMouseSensitivity, MaxMouseSensitivity);
		return (Clamped - MinMouseSensitivity) / (MaxMouseSensitivity - MinMouseSensitivity) * 100.f;
	}

	// 볼륨: 화면 0~100 ↔ 저장 0~1
	inline float VolumeFromPercent(float Percent) { return FMath::Clamp(Percent / 100.f, 0.f, 1.f); }
	inline float PercentFromVolume(float Volume) { return FMath::Clamp(Volume, 0.f, 1.f) * 100.f; }
}

/**
 * DaeRune 커스텀 설정 저장 클래스
 */
UCLASS()
class DAERUNE_API UDRGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	// 싱글톤 접근자 (엔진 초기화 전이면 null)
	static UDRGameUserSettings* GetDRGameUserSettings();

	// 커스텀 설정 적용
	void ApplyCustomSettings();

	// 기본값으로 초기화
	virtual void SetToDefaults() override;

public:
	UPROPERTY(Config, BlueprintReadWrite, Category = "Settings|Audio")
	float MasterVolume = DRSettingsDefaults::MasterVolumePercent / 100.f;

	UPROPERTY(Config, BlueprintReadWrite, Category = "Settings|Audio")
	float BGMVolume = DRSettingsDefaults::MusicVolumePercent / 100.f;

	UPROPERTY(Config, BlueprintReadWrite, Category = "Settings|Audio")
	float SFXVolume = DRSettingsDefaults::SFXVolumePercent / 100.f;

	UPROPERTY(Config, BlueprintReadWrite, Category = "Settings|Gameplay")
	float MouseSensitivity = 1.0f;   // SetToDefaults 가 DRSettingsDefaults 로 맞춘다

	UPROPERTY(Config, BlueprintReadWrite, Category = "Settings|Graphics")
	float Gamma = DRSettingsDefaults::GammaPercent;

	/** Preferred UI culture code: "ko" (Korean) or "en" (English). Saved to GameUserSettings.ini. */
	UPROPERTY(Config, BlueprintReadWrite, Category = "Settings|Localization")
	FString PreferredCulture = TEXT("ko");
};
