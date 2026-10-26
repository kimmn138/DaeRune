// Copyright DaeRune


#include "Game/DRSettingsManager.h"
#include "Game/DRGameUserSettings.h"
#include "Game/DRSettingsFunctionLibrary.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Internationalization/Internationalization.h"
#include "DaeRune/DRLogChannels.h"
#include <type_traits>

#define LOCTEXT_NAMESPACE "DRSettings"

// ========== 언어 매핑 SSOT ==========
// 지원 언어의 단일 진실 소스. 아래 GetSupportedLanguages()의 배열이 전부다:
//  - 드롭다운 옵션 생성, OptionId↔Culture 변환, 부팅 시 유효성 검사, 폴백 기본값 모두 이 배열에서 파생.
//  - 새 언어 추가 절차: (1) 이 배열에 엔트리 1줄 추가  (2) DefaultGame.ini의 +CulturesToStage에 컬처 코드 추가
//    (3) Localization Dashboard에서 해당 컬처를 Add + Gather/Compile.  → 코드 상으로는 (1) 한 줄이면 끝.

const TArray<FDRLanguageInfo>& UDRSettingsManager::GetSupportedLanguages()
{
	// 배열의 첫 항목이 기본 언어(현재: 한국어). DisplayText는 해당 언어의 자기 명칭(endonym)이라 번역하지 않는다.
	static const TArray<FDRLanguageInfo> SupportedLanguages = {
		{ FName("Korean"),  TEXT("ko"), FText::FromString(TEXT("한국어")) },
		{ FName("English"), TEXT("en"), FText::FromString(TEXT("English")) },
	};
	return SupportedLanguages;
}

const FDRLanguageInfo& UDRSettingsManager::GetDefaultLanguage()
{
	// GetSupportedLanguages()는 항상 1개 이상을 보장(컴파일 타임 상수 배열)하므로 [0] 안전.
	return GetSupportedLanguages()[0];
}

bool UDRSettingsManager::IsSupportedCulture(const FString& CultureCode)
{
	return CultureToLanguageOptionId(CultureCode) != NAME_None;
}

FString UDRSettingsManager::LanguageOptionIdToCulture(FName OptionId)
{
	for (const FDRLanguageInfo& Lang : GetSupportedLanguages())
	{
		if (Lang.OptionId == OptionId)
		{
			return Lang.CultureCode;
		}
	}
	return FString();
}

FName UDRSettingsManager::CultureToLanguageOptionId(const FString& CultureCode)
{
	// 엔진은 "ko-KR" 같은 지역 변형을 돌려줄 수 있으므로 컬처 코드 prefix로 매칭.
	// (향후 "zh-Hans"/"zh-Hant"처럼 접두가 겹치는 언어를 넣을 땐 더 구체적인 코드를 배열 앞쪽에 둘 것)
	for (const FDRLanguageInfo& Lang : GetSupportedLanguages())
	{
		if (CultureCode.StartsWith(Lang.CultureCode))
		{
			return Lang.OptionId;
		}
	}
	return NAME_None;
}

// ========== 설정 ID · 변환 헬퍼 ==========

namespace DRSettingIds
{
	const FName DisplayMode(TEXT("Graphics.DisplayMode"));
	const FName Resolution(TEXT("Graphics.Resolution"));
	const FName VSync(TEXT("Graphics.VSync"));
	const FName FPSLimit(TEXT("Graphics.FPSLimit"));
	const FName Scalability(TEXT("Graphics.Scalability"));
	const FName Gamma(TEXT("Graphics.Gamma"));
	const FName CameraSensitivity(TEXT("Gameplay.CameraSensitivity"));
	const FName Language(TEXT("Gameplay.Language"));
	const FName MasterVolume(TEXT("Audio.MasterVolume"));
	const FName MusicVolume(TEXT("Audio.MusicVolume"));
	const FName SFXVolume(TEXT("Audio.SFXVolume"));
}

namespace
{
	// 디스플레이 모드 선택지 — 옵션 순서 = SelectedIndex
	struct FDRDisplayModeOption
	{
		FName OptionId;
		EWindowMode::Type WindowMode;
	};

	const FDRDisplayModeOption DisplayModeOptions[] = {
		{ FName(TEXT("Fullscreen")), EWindowMode::Fullscreen },
		{ FName(TEXT("Borderless")), EWindowMode::WindowedFullscreen },
		{ FName(TEXT("Windowed")),   EWindowMode::Windowed },
	};

	int32 DisplayModeIndexFromWindowMode(EWindowMode::Type WindowMode)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(DisplayModeOptions); ++Index)
		{
			if (DisplayModeOptions[Index].WindowMode == WindowMode) return Index;
		}
		return 0;
	}

	bool WindowModeFromDisplayOption(FName OptionId, EWindowMode::Type& OutWindowMode)
	{
		for (const FDRDisplayModeOption& Option : DisplayModeOptions)
		{
			if (Option.OptionId == OptionId)
			{
				OutWindowMode = Option.WindowMode;
				return true;
			}
		}
		return false;
	}

	// 선택지 중 IntValue / OptionId 가 같은 것의 인덱스 (없으면 0)
	int32 FindOptionIndexByInt(const FDRSettingDefinition* Def, int32 IntValue)
	{
		if (!Def) return 0;
		const int32 Index = Def->Options.IndexOfByPredicate([IntValue](const FDRSettingsOption& Opt) { return Opt.IntValue == IntValue; });
		return FMath::Max(Index, 0);
	}

	int32 FindOptionIndexByName(const FDRSettingDefinition* Def, FName OptionId)
	{
		if (!Def) return 0;
		const int32 Index = Def->Options.IndexOfByPredicate([OptionId](const FDRSettingsOption& Opt) { return Opt.OptionId == OptionId; });
		return FMath::Max(Index, 0);
	}

	FDRSettingsValue MakeIndexedIntValue(int32 Value, int32 SelectedIndex)
	{
		FDRSettingsValue Result = UDRSettingsFunctionLibrary::MakeIntValue(Value);
		Result.SelectedIndex = SelectedIndex;
		return Result;
	}
}

// ========== 초기화 · 오디오 ==========

void UDRSettingsManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	auto LoadIfMissing = [](auto& Member, const TCHAR* Path)
	{
		using FAsset = std::remove_pointer_t<decltype(Member.Get())>;
		if (!Member)
		{
			Member = LoadObject<FAsset>(nullptr, Path);
		}
		if (!Member)
		{
			UE_LOG(LogDR, Warning, TEXT("[Settings] 오디오 에셋을 불러오지 못했습니다: %s"), Path);
		}
	};
	LoadIfMissing(GameSoundMix, TEXT("/Game/Blueprints/Audio/SoundMix/SM_GameMix.SM_GameMix"));
	LoadIfMissing(MasterSoundClass, TEXT("/Game/Blueprints/Audio/SoundClasses/SC_Master.SC_Master"));
	LoadIfMissing(BGMSoundClass, TEXT("/Game/Blueprints/Audio/SoundClasses/SC_BGM.SC_BGM"));
	LoadIfMissing(SFXSoundClass, TEXT("/Game/Blueprints/Audio/SoundClasses/SC_SFX.SC_SFX"));

	if (UDRGameUserSettings* Settings = GetSettings())
	{
		// 해상도 · 그래픽 품질 적용 (ApplySettings 가 해상도 외 설정까지 적용한다)
		Settings->ApplySettings(false);

		// 커스텀 설정(감마 등) 적용
		Settings->ApplyCustomSettings();
	}
}

UDRGameUserSettings* UDRSettingsManager::GetSettings() const
{
	return UDRGameUserSettings::GetDRGameUserSettings();
}

void UDRSettingsManager::ApplyVideoSettings()
{
	if (UDRGameUserSettings* Settings = GetSettings())
	{
		Settings->ApplyResolutionSettings(false);
		Settings->ConfirmVideoMode();
		Settings->ApplyNonResolutionSettings();
	}
}

void UDRSettingsManager::ApplyAudioSettings()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World && GEngine)
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

	if (World)
	{
		ApplySoundMixToWorld(World);
	}
}

void UDRSettingsManager::ApplySoundMixToWorld(UWorld* World)
{
	UDRGameUserSettings* Settings = GetSettings();
	if (!World || !Settings || !GameSoundMix) return;

	// ★Push 는 참조 카운트를 올린다★ — 볼륨을 바꿀 때마다 Push 만 하면 스택이 계속 쌓인다.
	// 한 번 빼고 다시 넣어 항상 하나만 유지한다 (없으면 Pop 은 아무 일도 안 한다)
	UGameplayStatics::PopSoundMixModifier(World, GameSoundMix);
	UGameplayStatics::PushSoundMixModifier(World, GameSoundMix);

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
		Settings->MouseSensitivity = FMath::Clamp(NewSensitivity,
			DRSettingsDefaults::MinMouseSensitivity, DRSettingsDefaults::MaxMouseSensitivity);
		BroadcastMouseSensitivity();
	}
}

void UDRSettingsManager::BroadcastMouseSensitivity()
{
	if (UDRGameUserSettings* Settings = GetSettings())
	{
		OnMouseSensitivityChanged.Broadcast(Settings->MouseSensitivity);
	}
}

TArray<FIntPoint> UDRSettingsManager::GetSupportedResolutions() const
{
	TArray<FIntPoint> Resolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);

	// 최소 해상도 필터링
	Resolutions.RemoveAll([](const FIntPoint& Res) { return Res.X < 1280 || Res.Y < 720; });
	return Resolutions;
}

void UDRSettingsManager::SetGraphicsQuality(int32 QualityLevel)
{
	if (UDRGameUserSettings* Settings = GetSettings())
	{
		// 0: Low, 1: Medium, 2: High, 3: Epic, 4: Cinematic
		Settings->SetOverallScalabilityLevel(FMath::Clamp(QualityLevel, 0, 4));
	}
}

// ========== 데이터 주도 관리 레이어 ==========

void UDRSettingsManager::InitSettings()
{
	if (bLoaded) return;

	BuildDefinitions();
	BuildDefaultValues();
	LoadFromGameUserSettings();

	// PendingValues = CurrentValues
	PendingValues = CurrentValues;

	bLoaded = true;
	OnSettingsLoaded.Broadcast();
}

FDRSettingDefinition& UDRSettingsManager::AddDefinition(FName SettingId, EDRSettingsTab Tab, int32 Order, const FText& Label,
	EDRSettingsControlType ControlType, EDRSettingsValueType ValueType, EDRSettingsApplyMode ApplyMode)
{
	FDRSettingDefinition& Def = Definitions.AddDefaulted_GetRef();
	Def.SettingId = SettingId;
	Def.Tab = Tab;
	Def.Order = Order;
	Def.LabelText = Label;
	Def.ControlType = ControlType;
	Def.ValueType = ValueType;
	Def.ApplyMode = ApplyMode;
	Def.bShowSeparator = true;
	Def.bEnabled = true;
	return Def;
}

void UDRSettingsManager::AddPercentSlider(FName SettingId, EDRSettingsTab Tab, int32 Order, const FText& Label, float DefaultPercent)
{
	FDRSettingDefinition& Def = AddDefinition(SettingId, Tab, Order, Label,
		EDRSettingsControlType::Slider, EDRSettingsValueType::Float, EDRSettingsApplyMode::Instant);
	Def.MinValue = 0.f;
	Def.MaxValue = 100.f;
	Def.StepValue = 1.f;
	Def.SuffixText = FText::FromString(TEXT("%"));
	Def.DefaultValue = UDRSettingsFunctionLibrary::MakeFloatValue(DefaultPercent);
}

void UDRSettingsManager::BuildDefinitions()
{
	Definitions.Empty();

	auto AddOption = [](FDRSettingDefinition& Def, FName OptionId, const FText& Text, int32 IntValue = 0)
	{
		FDRSettingsOption& Opt = Def.Options.AddDefaulted_GetRef();
		Opt.OptionId = OptionId;
		Opt.DisplayText = Text;
		Opt.IntValue = IntValue;
		return &Opt;
	};

	// ========== Graphics ==========

	{
		FDRSettingDefinition& Def = AddDefinition(DRSettingIds::DisplayMode, EDRSettingsTab::Graphics, 0,
			LOCTEXT("Settings_DisplayMode_Label", "디스플레이 모드"),
			EDRSettingsControlType::Dropdown, EDRSettingsValueType::Name, EDRSettingsApplyMode::RequiresApply);
		const FText DisplayModeLabels[] = {
			LOCTEXT("Settings_DisplayMode_Fullscreen", "전체 화면"),
			LOCTEXT("Settings_DisplayMode_Borderless", "테두리 없음"),
			LOCTEXT("Settings_DisplayMode_Windowed", "창 모드"),
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(DisplayModeOptions); ++Index)
		{
			AddOption(Def, DisplayModeOptions[Index].OptionId, DisplayModeLabels[Index], Index);
		}
		Def.DefaultValue = UDRSettingsFunctionLibrary::MakeNameValue(DisplayModeOptions[0].OptionId, 0);
	}

	{
		FDRSettingDefinition& Def = AddDefinition(DRSettingIds::Resolution, EDRSettingsTab::Graphics, 1,
			LOCTEXT("Settings_Resolution_Label", "해상도"),
			EDRSettingsControlType::Dropdown, EDRSettingsValueType::Resolution, EDRSettingsApplyMode::RequiresApply);
		for (const FIntPoint& Res : GetSupportedResolutions())
		{
			FDRSettingsOption* Opt = AddOption(Def, FName(*FString::Printf(TEXT("%dx%d"), Res.X, Res.Y)),
				FText::FromString(FString::Printf(TEXT("%d X %d"), Res.X, Res.Y)));
			Opt->ResolutionX = Res.X;
			Opt->ResolutionY = Res.Y;
		}
		Def.DefaultValue = UDRSettingsFunctionLibrary::MakeResolutionValue(1920, 1080, 0);
	}

	{
		FDRSettingDefinition& Def = AddDefinition(DRSettingIds::VSync, EDRSettingsTab::Graphics, 2,
			LOCTEXT("Settings_VSync_Label", "수직 동기화"),
			EDRSettingsControlType::Toggle, EDRSettingsValueType::Bool, EDRSettingsApplyMode::RequiresApply);
		Def.DefaultValue = UDRSettingsFunctionLibrary::MakeBoolValue(DRSettingsDefaults::bVSyncEnabled);
	}

	{
		FDRSettingDefinition& Def = AddDefinition(DRSettingIds::FPSLimit, EDRSettingsTab::Graphics, 3,
			LOCTEXT("Settings_FPSLimit_Label", "프레임 제한"),
			EDRSettingsControlType::Dropdown, EDRSettingsValueType::Int, EDRSettingsApplyMode::RequiresApply);
		AddOption(Def, FName(TEXT("30")), FText::FromString(TEXT("30")), 30);
		AddOption(Def, FName(TEXT("60")), FText::FromString(TEXT("60")), 60);
		AddOption(Def, FName(TEXT("120")), FText::FromString(TEXT("120")), 120);
		AddOption(Def, FName(TEXT("Unlimited")), LOCTEXT("Settings_FPS_Unlimited", "무제한"), 0);
		Def.DefaultValue = MakeIndexedIntValue(DRSettingsDefaults::FrameRateLimit,
			FindOptionIndexByInt(&Def, DRSettingsDefaults::FrameRateLimit));
	}

	{
		FDRSettingDefinition& Def = AddDefinition(DRSettingIds::Scalability, EDRSettingsTab::Graphics, 5,
			LOCTEXT("Settings_Quality_Label", "그래픽 품질"),
			EDRSettingsControlType::Dropdown, EDRSettingsValueType::Int, EDRSettingsApplyMode::RequiresApply);
		AddOption(Def, FName(TEXT("Low")), LOCTEXT("Settings_Quality_Low", "낮음"), 0);
		AddOption(Def, FName(TEXT("Medium")), LOCTEXT("Settings_Quality_Medium", "보통"), 1);
		AddOption(Def, FName(TEXT("High")), LOCTEXT("Settings_Quality_High", "높음"), 2);
		AddOption(Def, FName(TEXT("Epic")), LOCTEXT("Settings_Quality_Epic", "에픽"), 3);
		AddOption(Def, FName(TEXT("Cinematic")), LOCTEXT("Settings_Quality_Cinematic", "최상"), 4);
		// 값과 인덱스가 같다 (0~4)
		Def.DefaultValue = MakeIndexedIntValue(DRSettingsDefaults::ScalabilityLevel, DRSettingsDefaults::ScalabilityLevel);
	}

	AddPercentSlider(DRSettingIds::Gamma, EDRSettingsTab::Graphics, 4,
		LOCTEXT("Settings_Gamma_Label", "감마"), DRSettingsDefaults::GammaPercent);

	// ========== Gameplay ==========

	AddPercentSlider(DRSettingIds::CameraSensitivity, EDRSettingsTab::Gameplay, 0,
		LOCTEXT("Settings_CameraSensitivity_Label", "카메라 감도"), DRSettingsDefaults::CameraSensitivityPercent);

	{
		FDRSettingDefinition& Def = AddDefinition(DRSettingIds::Language, EDRSettingsTab::Gameplay, 1,
			LOCTEXT("Settings_Language_Label", "언어"),
			EDRSettingsControlType::Dropdown, EDRSettingsValueType::Name, EDRSettingsApplyMode::RequiresApply);

		// 지원 언어 SSOT를 순회해 드롭다운 옵션을 구성 (언어 추가는 GetSupportedLanguages()만 수정).
		for (const FDRLanguageInfo& Lang : GetSupportedLanguages())
		{
			AddOption(Def, Lang.OptionId, Lang.DisplayText);
		}

		// 기본값 = 목록 첫 항목(기본 언어), 인덱스 0.
		Def.DefaultValue = UDRSettingsFunctionLibrary::MakeNameValue(GetDefaultLanguage().OptionId, 0);
	}

	// ========== Audio ==========

	AddPercentSlider(DRSettingIds::MasterVolume, EDRSettingsTab::Audio, 0,
		LOCTEXT("Settings_MasterVolume_Label", "마스터 볼륨"), DRSettingsDefaults::MasterVolumePercent);
	AddPercentSlider(DRSettingIds::MusicVolume, EDRSettingsTab::Audio, 1,
		LOCTEXT("Settings_MusicVolume_Label", "음악 볼륨"), DRSettingsDefaults::MusicVolumePercent);
	AddPercentSlider(DRSettingIds::SFXVolume, EDRSettingsTab::Audio, 2,
		LOCTEXT("Settings_SFXVolume_Label", "효과음 볼륨"), DRSettingsDefaults::SFXVolumePercent);
}

void UDRSettingsManager::BuildDefaultValues()
{
	DefaultValues.Empty();

	for (const FDRSettingDefinition& Def : Definitions)
	{
		DefaultValues.Add(Def.SettingId, Def.DefaultValue);
	}
}

void UDRSettingsManager::LoadFromGameUserSettings()
{
	UDRGameUserSettings* Settings = GetSettings();
	if (!Settings) return;

	CurrentValues.Empty();

	// ===== Graphics =====
	{
		const int32 ModeIndex = DisplayModeIndexFromWindowMode(Settings->GetFullscreenMode());
		CurrentValues.Add(DRSettingIds::DisplayMode,
			UDRSettingsFunctionLibrary::MakeNameValue(DisplayModeOptions[ModeIndex].OptionId, ModeIndex));

		const FIntPoint Res = Settings->GetScreenResolution();
		const int32 ResIndex = FMath::Max(0, GetSupportedResolutions().IndexOfByKey(Res));
		CurrentValues.Add(DRSettingIds::Resolution, UDRSettingsFunctionLibrary::MakeResolutionValue(Res.X, Res.Y, ResIndex));

		CurrentValues.Add(DRSettingIds::VSync, UDRSettingsFunctionLibrary::MakeBoolValue(Settings->IsVSyncEnabled()));

		const int32 FPSInt = FMath::RoundToInt(Settings->GetFrameRateLimit());
		CurrentValues.Add(DRSettingIds::FPSLimit,
			MakeIndexedIntValue(FPSInt, FindOptionIndexByInt(FindDefinition(DRSettingIds::FPSLimit), FPSInt)));

		// 섞인 품질(-1)은 에픽으로 본다. 값과 인덱스가 같다 (0~4)
		int32 ScalabilityLevel = Settings->GetOverallScalabilityLevel();
		if (ScalabilityLevel < 0) ScalabilityLevel = DRSettingsDefaults::ScalabilityLevel;
		ScalabilityLevel = FMath::Clamp(ScalabilityLevel, 0, 4);
		CurrentValues.Add(DRSettingIds::Scalability, MakeIndexedIntValue(ScalabilityLevel, ScalabilityLevel));

		CurrentValues.Add(DRSettingIds::Gamma, UDRSettingsFunctionLibrary::MakeFloatValue(Settings->Gamma));
	}

	// ===== Gameplay =====
	{
		CurrentValues.Add(DRSettingIds::CameraSensitivity, UDRSettingsFunctionLibrary::MakeFloatValue(
			DRSettingsDefaults::PercentFromSensitivity(Settings->MouseSensitivity)));

		// Language: read PreferredCulture from GameUserSettings.ini and convert to OptionId.
		FName LangOptionId = CultureToLanguageOptionId(Settings->PreferredCulture);
		if (LangOptionId == NAME_None)
		{
			LangOptionId = GetDefaultLanguage().OptionId;
		}
		CurrentValues.Add(DRSettingIds::Language, UDRSettingsFunctionLibrary::MakeNameValue(
			LangOptionId, FindOptionIndexByName(FindDefinition(DRSettingIds::Language), LangOptionId)));
	}

	// ===== Audio =====
	CurrentValues.Add(DRSettingIds::MasterVolume, UDRSettingsFunctionLibrary::MakeFloatValue(DRSettingsDefaults::PercentFromVolume(Settings->MasterVolume)));
	CurrentValues.Add(DRSettingIds::MusicVolume, UDRSettingsFunctionLibrary::MakeFloatValue(DRSettingsDefaults::PercentFromVolume(Settings->BGMVolume)));
	CurrentValues.Add(DRSettingIds::SFXVolume, UDRSettingsFunctionLibrary::MakeFloatValue(DRSettingsDefaults::PercentFromVolume(Settings->SFXVolume)));
}

void UDRSettingsManager::WriteSettingToUserSettings(FName SettingId, const FDRSettingsValue& Value)
{
	UDRGameUserSettings* Settings = GetSettings();
	if (!Settings) return;

	if (SettingId == DRSettingIds::DisplayMode)
	{
		EWindowMode::Type WindowMode;
		if (WindowModeFromDisplayOption(Value.NameValue, WindowMode))
		{
			Settings->SetFullscreenMode(WindowMode);
		}
	}
	else if (SettingId == DRSettingIds::Resolution)
	{
		Settings->SetScreenResolution(FIntPoint(Value.ResolutionX, Value.ResolutionY));
	}
	else if (SettingId == DRSettingIds::VSync)
	{
		Settings->SetVSyncEnabled(Value.BoolValue);
	}
	else if (SettingId == DRSettingIds::FPSLimit)
	{
		Settings->SetFrameRateLimit(static_cast<float>(Value.IntValue));
	}
	else if (SettingId == DRSettingIds::Scalability)
	{
		SetGraphicsQuality(Value.IntValue);
	}
	else if (SettingId == DRSettingIds::Gamma)
	{
		Settings->Gamma = Value.FloatValue;
	}
	else if (SettingId == DRSettingIds::CameraSensitivity)
	{
		Settings->MouseSensitivity = DRSettingsDefaults::SensitivityFromPercent(Value.FloatValue);
	}
	else if (SettingId == DRSettingIds::Language)
	{
		// 모르는 값이면 기본 언어로
		const FString CultureCode = LanguageOptionIdToCulture(Value.NameValue);
		Settings->PreferredCulture = CultureCode.IsEmpty() ? GetDefaultLanguage().CultureCode : CultureCode;
	}
	else if (SettingId == DRSettingIds::MasterVolume)
	{
		SetMasterVolume(DRSettingsDefaults::VolumeFromPercent(Value.FloatValue));
	}
	else if (SettingId == DRSettingIds::MusicVolume)
	{
		SetBGMVolume(DRSettingsDefaults::VolumeFromPercent(Value.FloatValue));
	}
	else if (SettingId == DRSettingIds::SFXVolume)
	{
		SetSFXVolume(DRSettingsDefaults::VolumeFromPercent(Value.FloatValue));
	}
}

void UDRSettingsManager::SaveToGameUserSettings()
{
	UDRGameUserSettings* Settings = GetSettings();
	if (!Settings) return;

	for (const TPair<FName, FDRSettingsValue>& Pair : CurrentValues)
	{
		WriteSettingToUserSettings(Pair.Key, Pair.Value);
	}

	BroadcastMouseSensitivity();
	Settings->ApplyCustomSettings();
	Settings->SaveSettings();
}

TArray<FDRSettingDefinition> UDRSettingsManager::GetDefinitionsByTab(EDRSettingsTab Tab) const
{
	TArray<FDRSettingDefinition> Result = Definitions.FilterByPredicate(
		[Tab](const FDRSettingDefinition& Def) { return Def.Tab == Tab; });

	// Order로 정렬
	Result.Sort([](const FDRSettingDefinition& A, const FDRSettingDefinition& B)
	{
		return A.Order < B.Order;
	});

	return Result;
}

FDRSettingsValue UDRSettingsManager::GetPendingValue(FName SettingId) const
{
	if (const FDRSettingsValue* Val = PendingValues.Find(SettingId))
	{
		return *Val;
	}
	if (const FDRSettingsValue* Val = DefaultValues.Find(SettingId))
	{
		return *Val;
	}
	return FDRSettingsValue();
}

void UDRSettingsManager::SetPendingValue(FName SettingId, const FDRSettingsValue& NewValue)
{
	PendingValues.Add(SettingId, NewValue);

	// Dirty 상태 갱신
	const FDRSettingsValue* CurrentVal = CurrentValues.Find(SettingId);
	if (CurrentVal && UDRSettingsFunctionLibrary::IsSettingsValueEqual(*CurrentVal, NewValue))
	{
		DirtySettingIds.Remove(SettingId);
	}
	else
	{
		DirtySettingIds.AddUnique(SettingId);
	}

	UpdateHasPendingChanges();

	OnPendingValueChanged.Broadcast(SettingId, NewValue);

	// Instant 모드면 즉시 적용 (슬라이더 드래그마다 불리므로 정의는 복사하지 않고 찾는다)
	const FDRSettingDefinition* Def = FindDefinition(SettingId);
	if (Def && Def->ApplyMode == EDRSettingsApplyMode::Instant)
	{
		ApplySingleSetting(SettingId, NewValue);
		CommitSingleSetting(SettingId);
	}
}

void UDRSettingsManager::ApplyPendingSettings()
{
	if (bApplying) return;
	bApplying = true;

	for (const FName& SettingId : DirtySettingIds)
	{
		if (const FDRSettingsValue* Val = PendingValues.Find(SettingId))
		{
			ApplySingleSetting(SettingId, *Val);
		}
	}

	// CurrentValues = PendingValues
	CurrentValues = PendingValues;

	// GameUserSettings 에 저장한 뒤 해상도 · 창모드 · 품질을 한 번에 적용
	SaveToGameUserSettings();
	ApplyVideoSettings();

	DirtySettingIds.Empty();
	bHasPendingChanges = false;

	OnHasPendingChangesChanged.Broadcast(false);
	OnSettingsApplied.Broadcast();

	bApplying = false;
}

void UDRSettingsManager::ApplySingleSetting(FName SettingId, const FDRSettingsValue& Value)
{
	UDRGameUserSettings* Settings = GetSettings();
	if (!Settings) return;

	// 값은 GameUserSettings 에 쓰고, 즉시 반영이 필요한 것만 여기서 반영한다.
	// (해상도 · 창모드 · VSync · 프레임 · 품질은 ApplyPendingSettings 가 ApplyVideoSettings 로 한 번에 적용)
	WriteSettingToUserSettings(SettingId, Value);

	if (SettingId == DRSettingIds::Gamma)
	{
		Settings->ApplyCustomSettings();
	}
	else if (SettingId == DRSettingIds::CameraSensitivity)
	{
		BroadcastMouseSensitivity();
	}
	else if (SettingId == DRSettingIds::Language)
	{
		// 엔진 컬처 전환 — 이 호출 이후 LOCTEXT 조회가 새 언어를 쓴다
		FInternationalization::Get().SetCurrentLanguageAndLocale(Settings->PreferredCulture);
		Settings->SaveSettings();

		// 위젯이 표시 텍스트를 다시 그리도록 알린다
		OnLanguageChanged.Broadcast(Settings->PreferredCulture);
	}
	else if (SettingId == DRSettingIds::MasterVolume || SettingId == DRSettingIds::MusicVolume || SettingId == DRSettingIds::SFXVolume)
	{
		ApplyAudioSettings();
	}
}

void UDRSettingsManager::CommitSingleSetting(FName SettingId)
{
	if (const FDRSettingsValue* PendingVal = PendingValues.Find(SettingId))
	{
		CurrentValues.Add(SettingId, *PendingVal);
	}

	DirtySettingIds.Remove(SettingId);
	UpdateHasPendingChanges();
}

void UDRSettingsManager::ResetTabToDefault(EDRSettingsTab Tab, bool bApplyImmediately)
{
	for (const FDRSettingDefinition& Def : Definitions)
	{
		if (Def.Tab != Tab) continue;

		PendingValues.Add(Def.SettingId, Def.DefaultValue);

		const FDRSettingsValue* CurrentVal = CurrentValues.Find(Def.SettingId);
		if (CurrentVal && !UDRSettingsFunctionLibrary::IsSettingsValueEqual(*CurrentVal, Def.DefaultValue))
		{
			DirtySettingIds.AddUnique(Def.SettingId);
		}
		else
		{
			DirtySettingIds.Remove(Def.SettingId);
		}

		OnPendingValueChanged.Broadcast(Def.SettingId, Def.DefaultValue);
	}

	UpdateHasPendingChanges();
	OnSettingsReset.Broadcast(Tab);

	if (bApplyImmediately)
	{
		ApplyPendingSettings();
	}
}

void UDRSettingsManager::DiscardPendingChanges()
{
	PendingValues = CurrentValues;
	DirtySettingIds.Empty();
	bHasPendingChanges = false;

	OnHasPendingChangesChanged.Broadcast(false);

	for (const auto& Pair : CurrentValues)
	{
		OnPendingValueChanged.Broadcast(Pair.Key, Pair.Value);
	}
}

const FDRSettingDefinition* UDRSettingsManager::FindDefinition(FName SettingId) const
{
	return Definitions.FindByPredicate([SettingId](const FDRSettingDefinition& Def) { return Def.SettingId == SettingId; });
}

bool UDRSettingsManager::GetDefinitionById(FName SettingId, FDRSettingDefinition& OutDefinition) const
{
	if (const FDRSettingDefinition* Def = FindDefinition(SettingId))
	{
		OutDefinition = *Def;
		return true;
	}
	return false;
}

void UDRSettingsManager::UpdateHasPendingChanges()
{
	const bool bNewHasPending = DirtySettingIds.Num() > 0;
	if (bNewHasPending != bHasPendingChanges)
	{
		bHasPendingChanges = bNewHasPending;
		OnHasPendingChangesChanged.Broadcast(bHasPendingChanges);
	}
}

#undef LOCTEXT_NAMESPACE
