// Copyright DaeRune


#include "Game/DRSettingsManager.h"
#include "Game/DRGameUserSettings.h"
#include "Game/DRSettingsFunctionLibrary.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Character/DRCharacter.h"
#include "EngineUtils.h"
#include "Components/AudioComponent.h"
#include "Internationalization/Internationalization.h"

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

// ========== 기존 함수 (유지) ==========

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
    // 로드 실패 시 경고 로그
    if (!GameSoundMix)
    {
}
    if (!MasterSoundClass)
    {
}
    if (!BGMSoundClass)
    {
}
    if (!SFXSoundClass)
    {
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

    // ???
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

    // SoundMix 활성화
    UGameplayStatics::PushSoundMixModifier(World, GameSoundMix);

    // 각 클래스 볼륨 설정
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
        Settings->MouseSensitivity = FMath::Clamp(NewSensitivity, 0.1f, 5.0f);
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
        BroadcastMouseSensitivity();
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

void UDRSettingsManager::BuildDefinitions()
{
    Definitions.Empty();

    // ========== Graphics ==========

    // Graphics.DisplayMode
    {
        FDRSettingDefinition Def;
        Def.SettingId = FName("Graphics.DisplayMode");
        Def.Tab = EDRSettingsTab::Graphics;
        Def.Order = 0;
        Def.LabelText = LOCTEXT("Settings_DisplayMode_Label", "디스플레이 모드");
        Def.ControlType = EDRSettingsControlType::Dropdown;
        Def.ValueType = EDRSettingsValueType::Name;
        Def.ApplyMode = EDRSettingsApplyMode::RequiresApply;
        Def.bShowSeparator = true;
        Def.bEnabled = true;

        FDRSettingsOption FullscreenOpt;
        FullscreenOpt.OptionId = FName("Fullscreen");
        FullscreenOpt.DisplayText = LOCTEXT("Settings_DisplayMode_Fullscreen", "전체 화면");
        FullscreenOpt.IntValue = 0;
        Def.Options.Add(FullscreenOpt);

        FDRSettingsOption BorderlessOpt;
        BorderlessOpt.OptionId = FName("Borderless");
        BorderlessOpt.DisplayText = LOCTEXT("Settings_DisplayMode_Borderless", "테두리 없음");
        BorderlessOpt.IntValue = 1;
        Def.Options.Add(BorderlessOpt);

        FDRSettingsOption WindowedOpt;
        WindowedOpt.OptionId = FName("Windowed");
        WindowedOpt.DisplayText = LOCTEXT("Settings_DisplayMode_Windowed", "창 모드");
        WindowedOpt.IntValue = 2;
        Def.Options.Add(WindowedOpt);

        Def.DefaultValue = UDRSettingsFunctionLibrary::MakeNameValue(FName("Fullscreen"), 0);

        Definitions.Add(Def);
    }

    // Graphics.Resolution
    {
        FDRSettingDefinition Def;
        Def.SettingId = FName("Graphics.Resolution");
        Def.Tab = EDRSettingsTab::Graphics;
        Def.Order = 1;
        Def.LabelText = LOCTEXT("Settings_Resolution_Label", "해상도");
        Def.ControlType = EDRSettingsControlType::Dropdown;
        Def.ValueType = EDRSettingsValueType::Resolution;
        Def.ApplyMode = EDRSettingsApplyMode::RequiresApply;
        Def.bShowSeparator = true;
        Def.bEnabled = true;

        // 지원 해상도 동적 생성
        TArray<FIntPoint> Resolutions = GetSupportedResolutions();
        for (int32 i = 0; i < Resolutions.Num(); ++i)
        {
            FDRSettingsOption Opt;
            Opt.OptionId = FName(*FString::Printf(TEXT("%dx%d"), Resolutions[i].X, Resolutions[i].Y));
            Opt.DisplayText = FText::FromString(FString::Printf(TEXT("%d X %d"), Resolutions[i].X, Resolutions[i].Y));
            Opt.ResolutionX = Resolutions[i].X;
            Opt.ResolutionY = Resolutions[i].Y;
            Def.Options.Add(Opt);
        }

        Def.DefaultValue = UDRSettingsFunctionLibrary::MakeResolutionValue(1920, 1080, 0);

        Definitions.Add(Def);
    }

    // Graphics.VSync
    {
        FDRSettingDefinition Def;
        Def.SettingId = FName("Graphics.VSync");
        Def.Tab = EDRSettingsTab::Graphics;
        Def.Order = 2;
        Def.LabelText = LOCTEXT("Settings_VSync_Label", "수직 동기화");
        Def.ControlType = EDRSettingsControlType::Toggle;
        Def.ValueType = EDRSettingsValueType::Bool;
        Def.ApplyMode = EDRSettingsApplyMode::RequiresApply;
        Def.bShowSeparator = true;
        Def.bEnabled = true;
        Def.DefaultValue = UDRSettingsFunctionLibrary::MakeBoolValue(true);

        Definitions.Add(Def);
    }

    // Graphics.FPSLimit
    {
        FDRSettingDefinition Def;
        Def.SettingId = FName("Graphics.FPSLimit");
        Def.Tab = EDRSettingsTab::Graphics;
        Def.Order = 3;
        Def.LabelText = LOCTEXT("Settings_FPSLimit_Label", "프레임 제한");
        Def.ControlType = EDRSettingsControlType::Dropdown;
        Def.ValueType = EDRSettingsValueType::Int;
        Def.ApplyMode = EDRSettingsApplyMode::RequiresApply;
        Def.bShowSeparator = true;
        Def.bEnabled = true;

        const int32 FPSValues[] = { 30, 60, 120, 0 };
        const TCHAR* FPSOptionIds[] = { TEXT("30"), TEXT("60"), TEXT("120"), TEXT("Unlimited") };
        const FText FPSLabels[] = {
            FText::FromString(TEXT("30")),
            FText::FromString(TEXT("60")),
            FText::FromString(TEXT("120")),
            LOCTEXT("Settings_FPS_Unlimited", "무제한")
        };
        for (int32 i = 0; i < 4; ++i)
        {
            FDRSettingsOption Opt;
            Opt.OptionId = FName(FPSOptionIds[i]);
            Opt.DisplayText = FPSLabels[i];
            Opt.IntValue = FPSValues[i];
            Def.Options.Add(Opt);
        }

        {
            FDRSettingsValue DefaultVal = UDRSettingsFunctionLibrary::MakeIntValue(120);
            DefaultVal.SelectedIndex = 2; // 120은 [30,60,120,UNLIMITED] 중 인덱스 2
            Def.DefaultValue = DefaultVal;
        }

        Definitions.Add(Def);
    }

    // Graphics.Scalability
    {
        FDRSettingDefinition Def;
        Def.SettingId = FName("Graphics.Scalability");
        Def.Tab = EDRSettingsTab::Graphics;
        Def.Order = 5;
        Def.LabelText = LOCTEXT("Settings_Quality_Label", "그래픽 품질");
        Def.ControlType = EDRSettingsControlType::Dropdown;
        Def.ValueType = EDRSettingsValueType::Int;
        Def.ApplyMode = EDRSettingsApplyMode::RequiresApply;
        Def.bShowSeparator = true;
        Def.bEnabled = true;

        const int32 QualityValues[] = { 0, 1, 2, 3, 4 };
        const TCHAR* QualityOptionIds[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
        const FText QualityLabels[] = {
            LOCTEXT("Settings_Quality_Low", "낮음"),
            LOCTEXT("Settings_Quality_Medium", "보통"),
            LOCTEXT("Settings_Quality_High", "높음"),
            LOCTEXT("Settings_Quality_Epic", "에픽"),
            LOCTEXT("Settings_Quality_Cinematic", "최상")
        };
        for (int32 i = 0; i < 5; ++i)
        {
            FDRSettingsOption Opt;
            Opt.OptionId = FName(QualityOptionIds[i]);
            Opt.DisplayText = QualityLabels[i];
            Opt.IntValue = QualityValues[i];
            Def.Options.Add(Opt);
        }

        {
            FDRSettingsValue DefaultVal = UDRSettingsFunctionLibrary::MakeIntValue(3);
            DefaultVal.SelectedIndex = 3; // Epic
            Def.DefaultValue = DefaultVal;
        }

        Definitions.Add(Def);
    }

    // Graphics.Gamma
    {
        FDRSettingDefinition Def;
        Def.SettingId = FName("Graphics.Gamma");
        Def.Tab = EDRSettingsTab::Graphics;
        Def.Order = 4;
        Def.LabelText = LOCTEXT("Settings_Gamma_Label", "감마");
        Def.ControlType = EDRSettingsControlType::Slider;
        Def.ValueType = EDRSettingsValueType::Float;
        Def.MinValue = 0.f;
        Def.MaxValue = 100.f;
        Def.StepValue = 1.f;
        Def.SuffixText = FText::FromString(TEXT("%"));
        Def.ApplyMode = EDRSettingsApplyMode::Instant;
        Def.bShowSeparator = true;
        Def.bEnabled = true;
        Def.DefaultValue = UDRSettingsFunctionLibrary::MakeFloatValue(80.f);

        Definitions.Add(Def);
    }

    // ========== Gameplay ==========

    // Gameplay.CameraSensitivity
    {
        FDRSettingDefinition Def;
        Def.SettingId = FName("Gameplay.CameraSensitivity");
        Def.Tab = EDRSettingsTab::Gameplay;
        Def.Order = 0;
        Def.LabelText = LOCTEXT("Settings_CameraSensitivity_Label", "카메라 감도");
        Def.ControlType = EDRSettingsControlType::Slider;
        Def.ValueType = EDRSettingsValueType::Float;
        Def.MinValue = 0.f;
        Def.MaxValue = 100.f;
        Def.StepValue = 1.f;
        Def.SuffixText = FText::FromString(TEXT("%"));
        Def.ApplyMode = EDRSettingsApplyMode::Instant;
        Def.bShowSeparator = true;
        Def.bEnabled = true;
        Def.DefaultValue = UDRSettingsFunctionLibrary::MakeFloatValue(80.f);

        Definitions.Add(Def);
    }

    // Gameplay.Language
    {
        FDRSettingDefinition Def;
        Def.SettingId = FName("Gameplay.Language");
        Def.Tab = EDRSettingsTab::Gameplay;
        Def.Order = 1;
        Def.LabelText = LOCTEXT("Settings_Language_Label", "언어");
        Def.ControlType = EDRSettingsControlType::Dropdown;
        Def.ValueType = EDRSettingsValueType::Name;
        Def.ApplyMode = EDRSettingsApplyMode::RequiresApply;
        Def.bShowSeparator = true;
        Def.bEnabled = true;

        // 지원 언어 SSOT를 순회해 드롭다운 옵션을 구성 (언어 추가는 GetSupportedLanguages()만 수정).
        for (const FDRLanguageInfo& Lang : GetSupportedLanguages())
        {
            FDRSettingsOption Opt;
            Opt.OptionId = Lang.OptionId;
            Opt.DisplayText = Lang.DisplayText;
            Def.Options.Add(Opt);
        }

        // 기본값 = 목록 첫 항목(기본 언어), 인덱스 0.
        Def.DefaultValue = UDRSettingsFunctionLibrary::MakeNameValue(GetDefaultLanguage().OptionId, 0);

        Definitions.Add(Def);
    }

    // ========== Audio ==========

    // Audio.MasterVolume
    {
        FDRSettingDefinition Def;
        Def.SettingId = FName("Audio.MasterVolume");
        Def.Tab = EDRSettingsTab::Audio;
        Def.Order = 0;
        Def.LabelText = LOCTEXT("Settings_MasterVolume_Label", "마스터 볼륨");
        Def.ControlType = EDRSettingsControlType::Slider;
        Def.ValueType = EDRSettingsValueType::Float;
        Def.MinValue = 0.f;
        Def.MaxValue = 100.f;
        Def.StepValue = 1.f;
        Def.SuffixText = FText::FromString(TEXT("%"));
        Def.ApplyMode = EDRSettingsApplyMode::Instant;
        Def.bShowSeparator = true;
        Def.bEnabled = true;
        Def.DefaultValue = UDRSettingsFunctionLibrary::MakeFloatValue(100.f);

        Definitions.Add(Def);
    }

    // Audio.MusicVolume
    {
        FDRSettingDefinition Def;
        Def.SettingId = FName("Audio.MusicVolume");
        Def.Tab = EDRSettingsTab::Audio;
        Def.Order = 1;
        Def.LabelText = LOCTEXT("Settings_MusicVolume_Label", "음악 볼륨");
        Def.ControlType = EDRSettingsControlType::Slider;
        Def.ValueType = EDRSettingsValueType::Float;
        Def.MinValue = 0.f;
        Def.MaxValue = 100.f;
        Def.StepValue = 1.f;
        Def.SuffixText = FText::FromString(TEXT("%"));
        Def.ApplyMode = EDRSettingsApplyMode::Instant;
        Def.bShowSeparator = true;
        Def.bEnabled = true;
        Def.DefaultValue = UDRSettingsFunctionLibrary::MakeFloatValue(80.f);

        Definitions.Add(Def);
    }

    // Audio.SFXVolume
    {
        FDRSettingDefinition Def;
        Def.SettingId = FName("Audio.SFXVolume");
        Def.Tab = EDRSettingsTab::Audio;
        Def.Order = 2;
        Def.LabelText = LOCTEXT("Settings_SFXVolume_Label", "효과음 볼륨");
        Def.ControlType = EDRSettingsControlType::Slider;
        Def.ValueType = EDRSettingsValueType::Float;
        Def.MinValue = 0.f;
        Def.MaxValue = 100.f;
        Def.StepValue = 1.f;
        Def.SuffixText = FText::FromString(TEXT("%"));
        Def.ApplyMode = EDRSettingsApplyMode::Instant;
        Def.bShowSeparator = true;
        Def.bEnabled = true;
        Def.DefaultValue = UDRSettingsFunctionLibrary::MakeFloatValue(80.f);

        Definitions.Add(Def);
    }

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

    // Graphics
    {
        // DisplayMode
        EWindowMode::Type WinMode = Settings->GetFullscreenMode();
        FName ModeName;
        int32 ModeIndex = 0;
        switch (WinMode)
        {
        case EWindowMode::Fullscreen:
            ModeName = FName("Fullscreen");
            ModeIndex = 0;
            break;
        case EWindowMode::WindowedFullscreen:
            ModeName = FName("Borderless");
            ModeIndex = 1;
            break;
        case EWindowMode::Windowed:
            ModeName = FName("Windowed");
            ModeIndex = 2;
            break;
        default:
            ModeName = FName("Fullscreen");
            ModeIndex = 0;
            break;
        }
        CurrentValues.Add(FName("Graphics.DisplayMode"), UDRSettingsFunctionLibrary::MakeNameValue(ModeName, ModeIndex));

        // Resolution
        FIntPoint Res = Settings->GetScreenResolution();
        // SelectedIndex 계산
        TArray<FIntPoint> SupportedRes = GetSupportedResolutions();
        int32 ResIndex = 0;
        for (int32 i = 0; i < SupportedRes.Num(); ++i)
        {
            if (SupportedRes[i] == Res)
            {
                ResIndex = i;
                break;
            }
        }
        CurrentValues.Add(FName("Graphics.Resolution"), UDRSettingsFunctionLibrary::MakeResolutionValue(Res.X, Res.Y, ResIndex));

        // VSync
        CurrentValues.Add(FName("Graphics.VSync"), UDRSettingsFunctionLibrary::MakeBoolValue(Settings->IsVSyncEnabled()));

        // FPSLimit (SelectedIndex를 Options 배열 매칭으로 계산)
        float FPSLimit = Settings->GetFrameRateLimit();
        int32 FPSInt = FMath::RoundToInt(FPSLimit);
        int32 FPSIndex = 0;
        FDRSettingDefinition FPSDef;
        if (GetDefinitionById(FName("Graphics.FPSLimit"), FPSDef))
        {
            for (int32 i = 0; i < FPSDef.Options.Num(); ++i)
            {
                if (FPSDef.Options[i].IntValue == FPSInt)
                {
                    FPSIndex = i;
                    break;
                }
            }
        }
        {
            FDRSettingsValue FPSValue = UDRSettingsFunctionLibrary::MakeIntValue(FPSInt);
            FPSValue.SelectedIndex = FPSIndex;
            CurrentValues.Add(FName("Graphics.FPSLimit"), FPSValue);
        }

        // Scalability
        {
            int32 ScalabilityLevel = Settings->GetOverallScalabilityLevel();
            if (ScalabilityLevel < 0) ScalabilityLevel = 3; // Mixed → Epic 기본값
            ScalabilityLevel = FMath::Clamp(ScalabilityLevel, 0, 4);
            FDRSettingsValue ScalVal = UDRSettingsFunctionLibrary::MakeIntValue(ScalabilityLevel);
            ScalVal.SelectedIndex = ScalabilityLevel; // 값과 인덱스가 동일 (0~4)
            CurrentValues.Add(FName("Graphics.Scalability"), ScalVal);
        }

        // Gamma
        CurrentValues.Add(FName("Graphics.Gamma"), UDRSettingsFunctionLibrary::MakeFloatValue(Settings->Gamma));
    }

    // Gameplay
    {
        // CameraSensitivity: 0.1~5.0 → 0~100 변환
        float NormalizedSens = ((Settings->MouseSensitivity - 0.1f) / (5.0f - 0.1f)) * 100.f;
        CurrentValues.Add(FName("Gameplay.CameraSensitivity"), UDRSettingsFunctionLibrary::MakeFloatValue(NormalizedSens));

        // Language: read PreferredCulture from GameUserSettings.ini and convert to OptionId.
        FName LangOptionId = CultureToLanguageOptionId(Settings->PreferredCulture);
        if (LangOptionId == NAME_None)
        {
            LangOptionId = GetDefaultLanguage().OptionId;
        }
        // Compute SelectedIndex from the Definitions array.
        int32 LangIndex = 0;
        FDRSettingDefinition LangDef;
        if (GetDefinitionById(FName("Gameplay.Language"), LangDef))
        {
            for (int32 i = 0; i < LangDef.Options.Num(); ++i)
            {
                if (LangDef.Options[i].OptionId == LangOptionId)
                {
                    LangIndex = i;
                    break;
                }
            }
        }
        CurrentValues.Add(FName("Gameplay.Language"), UDRSettingsFunctionLibrary::MakeNameValue(LangOptionId, LangIndex));
    }

    // Audio: 0.0~1.0 → 0~100 변환
    {
        CurrentValues.Add(FName("Audio.MasterVolume"), UDRSettingsFunctionLibrary::MakeFloatValue(Settings->MasterVolume * 100.f));
        CurrentValues.Add(FName("Audio.MusicVolume"), UDRSettingsFunctionLibrary::MakeFloatValue(Settings->BGMVolume * 100.f));
        CurrentValues.Add(FName("Audio.SFXVolume"), UDRSettingsFunctionLibrary::MakeFloatValue(Settings->SFXVolume * 100.f));
    }
}

void UDRSettingsManager::SaveToGameUserSettings()
{
    UDRGameUserSettings* Settings = GetSettings();
    if (!Settings) return;

    // Graphics.DisplayMode
    if (const FDRSettingsValue* Val = CurrentValues.Find(FName("Graphics.DisplayMode")))
    {
        if (Val->NameValue == FName("Fullscreen"))
        {
            Settings->SetFullscreenMode(EWindowMode::Fullscreen);
        }
        else if (Val->NameValue == FName("Borderless"))
        {
            Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
        }
        else if (Val->NameValue == FName("Windowed"))
        {
            Settings->SetFullscreenMode(EWindowMode::Windowed);
        }
    }

    // Graphics.Resolution
    if (const FDRSettingsValue* Val = CurrentValues.Find(FName("Graphics.Resolution")))
    {
        Settings->SetScreenResolution(FIntPoint(Val->ResolutionX, Val->ResolutionY));
    }

    // Graphics.VSync
    if (const FDRSettingsValue* Val = CurrentValues.Find(FName("Graphics.VSync")))
    {
        Settings->SetVSyncEnabled(Val->BoolValue);
    }

    // Graphics.FPSLimit
    if (const FDRSettingsValue* Val = CurrentValues.Find(FName("Graphics.FPSLimit")))
    {
        Settings->SetFrameRateLimit(static_cast<float>(Val->IntValue));
    }

    // Graphics.Scalability
    if (const FDRSettingsValue* Val = CurrentValues.Find(FName("Graphics.Scalability")))
    {
        Settings->SetOverallScalabilityLevel(Val->IntValue);
    }

    // Graphics.Gamma
    if (const FDRSettingsValue* Val = CurrentValues.Find(FName("Graphics.Gamma")))
    {
        Settings->Gamma = Val->FloatValue;
    }

    // Gameplay.CameraSensitivity: 0~100 ??0.1~5.0
    if (const FDRSettingsValue* Val = CurrentValues.Find(FName("Gameplay.CameraSensitivity")))
    {
        float Normalized = Val->FloatValue / 100.f;
        Settings->MouseSensitivity = FMath::Lerp(0.1f, 5.0f, Normalized);
        BroadcastMouseSensitivity();
    }

    // Gameplay.Language: OptionId -> Culture, store on GameUserSettings.
    if (const FDRSettingsValue* Val = CurrentValues.Find(FName("Gameplay.Language")))
    {
        const FString CultureCode = LanguageOptionIdToCulture(Val->NameValue);
        if (!CultureCode.IsEmpty())
        {
            Settings->PreferredCulture = CultureCode;
        }
    }

    // Audio: 0~100 ??0.0~1.0
    if (const FDRSettingsValue* Val = CurrentValues.Find(FName("Audio.MasterVolume")))
    {
        Settings->MasterVolume = Val->FloatValue / 100.f;
    }
    if (const FDRSettingsValue* Val = CurrentValues.Find(FName("Audio.MusicVolume")))
    {
        Settings->BGMVolume = Val->FloatValue / 100.f;
    }
    if (const FDRSettingsValue* Val = CurrentValues.Find(FName("Audio.SFXVolume")))
    {
        Settings->SFXVolume = Val->FloatValue / 100.f;
    }
    Settings->ApplyCustomSettings();
    Settings->SaveSettings();
}

TArray<FDRSettingDefinition> UDRSettingsManager::GetDefinitionsByTab(EDRSettingsTab Tab) const
{
    TArray<FDRSettingDefinition> Result;

    for (const FDRSettingDefinition& Def : Definitions)
    {
        if (Def.Tab == Tab)
        {
            Result.Add(Def);
        }
    }

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

    // Instant 모드면 즉시 적용
    FDRSettingDefinition Def;
    if (GetDefinitionById(SettingId, Def))
    {
        if (Def.ApplyMode == EDRSettingsApplyMode::Instant)
        {
            ApplySingleSetting(SettingId, NewValue);
            CommitSingleSetting(SettingId);
        }
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

    // GameUserSettings?????
    SaveToGameUserSettings();

    // 해상도/창모드 적용
    ApplyResolutionSettings();
    ApplyNonResolutionSettings();

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

    if (SettingId == FName("Graphics.DisplayMode"))
    {
        if (Value.NameValue == FName("Fullscreen"))
        {
            Settings->SetFullscreenMode(EWindowMode::Fullscreen);
        }
        else if (Value.NameValue == FName("Borderless"))
        {
            Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
        }
        else if (Value.NameValue == FName("Windowed"))
        {
            Settings->SetFullscreenMode(EWindowMode::Windowed);
        }
    }
    else if (SettingId == FName("Graphics.Resolution"))
    {
        Settings->SetScreenResolution(FIntPoint(Value.ResolutionX, Value.ResolutionY));
    }
    else if (SettingId == FName("Graphics.VSync"))
    {
        Settings->SetVSyncEnabled(Value.BoolValue);
    }
    else if (SettingId == FName("Graphics.FPSLimit"))
    {
        Settings->SetFrameRateLimit(static_cast<float>(Value.IntValue));
    }
    else if (SettingId == FName("Graphics.Scalability"))
    {
        SetGraphicsQuality(Value.IntValue);
        ApplyGraphicsQualitySettings();
    }
    else if (SettingId == FName("Graphics.Gamma"))
    {
        Settings->Gamma = Value.FloatValue;
        Settings->ApplyCustomSettings();
    }
    else if (SettingId == FName("Gameplay.CameraSensitivity"))
    {
        // 0~100 ??0.1~5.0
        float Normalized = Value.FloatValue / 100.f;
        SetMouseSensitivity(FMath::Lerp(0.1f, 5.0f, Normalized));
    }
    else if (SettingId == FName("Gameplay.Language"))
    {
        // 1) OptionId -> Culture mapping. Unknown value falls back to default (Korean).
        FName OptionId = Value.NameValue;
        FString CultureCode = LanguageOptionIdToCulture(OptionId);
        if (CultureCode.IsEmpty())
        {
            OptionId = GetDefaultLanguage().OptionId;
            CultureCode = GetDefaultLanguage().CultureCode;
        }

        // 2) Switch engine culture. From this call onward LOCTEXT lookups use the new language.
        FInternationalization::Get().SetCurrentLanguageAndLocale(CultureCode);

        // 3) Persist to GameUserSettings.ini.
        Settings->PreferredCulture = CultureCode;
        Settings->SaveSettings();

        // 4) Notify subscribers so widgets can refresh their displayed text.
        OnLanguageChanged.Broadcast(CultureCode);
    }
    else if (SettingId == FName("Audio.MasterVolume"))
    {
        SetMasterVolume(Value.FloatValue / 100.f);
        ApplyAudioSettings();
    }
    else if (SettingId == FName("Audio.MusicVolume"))
    {
        SetBGMVolume(Value.FloatValue / 100.f);
        ApplyAudioSettings();
    }
    else if (SettingId == FName("Audio.SFXVolume"))
    {
        SetSFXVolume(Value.FloatValue / 100.f);
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
    TArray<FDRSettingDefinition> TabDefs = GetDefinitionsByTab(Tab);

    for (const FDRSettingDefinition& Def : TabDefs)
    {
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

bool UDRSettingsManager::IsSettingDirty(FName SettingId) const
{
    return DirtySettingIds.Contains(SettingId);
}

bool UDRSettingsManager::GetDefinitionById(FName SettingId, FDRSettingDefinition& OutDefinition) const
{
    for (const FDRSettingDefinition& Def : Definitions)
    {
        if (Def.SettingId == SettingId)
        {
            OutDefinition = Def;
            return true;
        }
    }
    return false;
}

void UDRSettingsManager::UpdateHasPendingChanges()
{
    bool bNewHasPending = DirtySettingIds.Num() > 0;
    if (bNewHasPending != bHasPendingChanges)
    {
        bHasPendingChanges = bNewHasPending;
        OnHasPendingChangesChanged.Broadcast(bHasPendingChanges);
    }
}

#undef LOCTEXT_NAMESPACE

