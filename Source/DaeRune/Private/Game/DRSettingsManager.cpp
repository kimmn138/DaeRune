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

// ========== 湲곗〈 ?⑥닔 (?좎?) ==========

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
    // 濡쒕뱶 ?ㅽ뙣 ??寃쎄퀬 濡쒓렇
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
        // 湲곕낯 ?ㅼ젙 ?곸슜
        Settings->ApplySettings(false);

        // 而ㅼ뒪? ?ㅼ젙 ?곸슜
        Settings->ApplyCustomSettings();

        // ??λ맂 Scalability ?ㅼ젙 ?곸슜
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

    // 而ㅼ뒪? ?ㅼ젙 ?좏슚??寃??
    Settings->ApplyCustomSettings();

    // ?댁긽??李쎈え???곸슜
    Settings->ApplyResolutionSettings(false);
    Settings->ApplyNonResolutionSettings();
    Settings->ConfirmVideoMode();

    // 洹몃옒???덉쭏 ?곸슜
    ApplyGraphicsQualitySettings();

    // ?ㅻ뵒???곸슜
    ApplyAudioSettings();

    // ???
    Settings->SaveSettings();

    // ?몃━寃뚯씠??釉뚮줈?쒖틦?ㅽ듃
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

    // SoundMix ?쒖꽦??
    UGameplayStatics::PushSoundMixModifier(World, GameSoundMix);

    // 媛??대옒??蹂쇰ⅷ ?ㅼ젙
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

    // 理쒖냼 ?댁긽???꾪꽣留?
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
        // Scalability ?ㅼ젙 ?곸슜
        Settings->ApplyNonResolutionSettings();
    }
}

// ========== ?곗씠??二쇰룄 愿由??덉씠??==========

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
        Def.LabelText = FText::FromString(TEXT("DISPLAY MODE"));
        Def.ControlType = EDRSettingsControlType::Dropdown;
        Def.ValueType = EDRSettingsValueType::Name;
        Def.ApplyMode = EDRSettingsApplyMode::RequiresApply;
        Def.bShowSeparator = true;
        Def.bEnabled = true;

        FDRSettingsOption FullscreenOpt;
        FullscreenOpt.OptionId = FName("Fullscreen");
        FullscreenOpt.DisplayText = FText::FromString(TEXT("FULLSCREEN"));
        FullscreenOpt.IntValue = 0;
        Def.Options.Add(FullscreenOpt);

        FDRSettingsOption BorderlessOpt;
        BorderlessOpt.OptionId = FName("Borderless");
        BorderlessOpt.DisplayText = FText::FromString(TEXT("BORDERLESS"));
        BorderlessOpt.IntValue = 1;
        Def.Options.Add(BorderlessOpt);

        FDRSettingsOption WindowedOpt;
        WindowedOpt.OptionId = FName("Windowed");
        WindowedOpt.DisplayText = FText::FromString(TEXT("WINDOWED"));
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
        Def.LabelText = FText::FromString(TEXT("RESOLUTION"));
        Def.ControlType = EDRSettingsControlType::Dropdown;
        Def.ValueType = EDRSettingsValueType::Resolution;
        Def.ApplyMode = EDRSettingsApplyMode::RequiresApply;
        Def.bShowSeparator = true;
        Def.bEnabled = true;

        // 吏???댁긽???숈쟻 ?앹꽦
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
        Def.LabelText = FText::FromString(TEXT("VSYNC"));
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
        Def.LabelText = FText::FromString(TEXT("FPS LIMIT"));
        Def.ControlType = EDRSettingsControlType::Dropdown;
        Def.ValueType = EDRSettingsValueType::Int;
        Def.ApplyMode = EDRSettingsApplyMode::RequiresApply;
        Def.bShowSeparator = true;
        Def.bEnabled = true;

        const int32 FPSValues[] = { 30, 60, 120, 0 };
        const TCHAR* FPSLabels[] = { TEXT("30"), TEXT("60"), TEXT("120"), TEXT("UNLIMITED") };
        for (int32 i = 0; i < 4; ++i)
        {
            FDRSettingsOption Opt;
            Opt.OptionId = FName(FPSLabels[i]);
            Opt.DisplayText = FText::FromString(FPSLabels[i]);
            Opt.IntValue = FPSValues[i];
            Def.Options.Add(Opt);
        }

        {
            FDRSettingsValue DefaultVal = UDRSettingsFunctionLibrary::MakeIntValue(120);
            DefaultVal.SelectedIndex = 2; // 120? [30,60,120,UNLIMITED] 以??몃뜳??2
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
        Def.LabelText = FText::FromString(TEXT("GRAPHICS QUALITY"));
        Def.ControlType = EDRSettingsControlType::Dropdown;
        Def.ValueType = EDRSettingsValueType::Int;
        Def.ApplyMode = EDRSettingsApplyMode::RequiresApply;
        Def.bShowSeparator = true;
        Def.bEnabled = true;

        const int32 QualityValues[] = { 0, 1, 2, 3, 4 };
        const TCHAR* QualityLabels[] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC"), TEXT("CINEMATIC") };
        for (int32 i = 0; i < 5; ++i)
        {
            FDRSettingsOption Opt;
            Opt.OptionId = FName(QualityLabels[i]);
            Opt.DisplayText = FText::FromString(QualityLabels[i]);
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
        Def.LabelText = FText::FromString(TEXT("GAMMA"));
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
        Def.LabelText = FText::FromString(TEXT("CAMERA SENSITIVITY"));
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
        Def.LabelText = FText::FromString(TEXT("LANGUAGE"));
        Def.ControlType = EDRSettingsControlType::Dropdown;
        Def.ValueType = EDRSettingsValueType::Name;
        Def.ApplyMode = EDRSettingsApplyMode::RequiresApply;
        Def.bShowSeparator = true;
        Def.bEnabled = true;

        FDRSettingsOption KoreanOpt;
        KoreanOpt.OptionId = FName("Korean");
        KoreanOpt.DisplayText = FText::FromString(TEXT("KOREAN"));
        Def.Options.Add(KoreanOpt);

        FDRSettingsOption ChineseOpt;
        ChineseOpt.OptionId = FName("Chinese");
        ChineseOpt.DisplayText = FText::FromString(TEXT("CHINESE"));
        Def.Options.Add(ChineseOpt);

        FDRSettingsOption EnglishOpt;
        EnglishOpt.OptionId = FName("English");
        EnglishOpt.DisplayText = FText::FromString(TEXT("ENGLISH"));
        Def.Options.Add(EnglishOpt);

        Def.DefaultValue = UDRSettingsFunctionLibrary::MakeNameValue(FName("Korean"), 0);

        Definitions.Add(Def);
    }

    // ========== Audio ==========

    // Audio.MasterVolume
    {
        FDRSettingDefinition Def;
        Def.SettingId = FName("Audio.MasterVolume");
        Def.Tab = EDRSettingsTab::Audio;
        Def.Order = 0;
        Def.LabelText = FText::FromString(TEXT("MASTER VOLUME"));
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
        Def.LabelText = FText::FromString(TEXT("MUSIC VOLUME"));
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
        Def.LabelText = FText::FromString(TEXT("SFX VOLUME"));
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
        // SelectedIndex 怨꾩궛
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

        // FPSLimit (SelectedIndex瑜?Options 諛곗뿴 留ㅼ묶?쇰줈 怨꾩궛)
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
            if (ScalabilityLevel < 0) ScalabilityLevel = 3; // Mixed ??Epic 湲곕낯媛?
            ScalabilityLevel = FMath::Clamp(ScalabilityLevel, 0, 4);
            FDRSettingsValue ScalVal = UDRSettingsFunctionLibrary::MakeIntValue(ScalabilityLevel);
            ScalVal.SelectedIndex = ScalabilityLevel; // 媛믨낵 ?몃뜳?ㅺ? ?숈씪 (0~4)
            CurrentValues.Add(FName("Graphics.Scalability"), ScalVal);
        }

        // Gamma
        CurrentValues.Add(FName("Graphics.Gamma"), UDRSettingsFunctionLibrary::MakeFloatValue(Settings->Gamma));
    }

    // Gameplay
    {
        // CameraSensitivity: 0.1~5.0 ??0~100 蹂??
        float NormalizedSens = ((Settings->MouseSensitivity - 0.1f) / (5.0f - 0.1f)) * 100.f;
        CurrentValues.Add(FName("Gameplay.CameraSensitivity"), UDRSettingsFunctionLibrary::MakeFloatValue(NormalizedSens));

        // Language: 湲곕낯媛??ъ슜 (?꾩옱 GameUserSettings???놁쓬)
        CurrentValues.Add(FName("Gameplay.Language"), UDRSettingsFunctionLibrary::MakeNameValue(FName("Korean"), 0));
    }

    // Audio: 0.0~1.0 ??0~100 蹂??
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

    // Order濡??뺣젹
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

    // Dirty ?곹깭 媛깆떊
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

    // Instant 紐⑤뱶硫?利됱떆 ?곸슜
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

    // ?댁긽??李쎈え???곸슜
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
        // ??λ쭔 (濡쒖뺄?쇱씠?쒖씠???쒖뒪???곌껐? 異뷀썑)
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

