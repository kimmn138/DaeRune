// Copyright DaeRune


#include "UI/Widget/DRSettingsWidget.h"
#include "Game/DRGameUserSettings.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DRPlayerController.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Game/DRSettingsManager.h"

void UDRSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // �� ��ư ���ε�
    if (Button_Sound)
    {
        Button_Sound->OnClicked.AddDynamic(this, &UDRSettingsWidget::OnSoundTabClicked);
    }
    if (Button_Graphics)
    {
        Button_Graphics->OnClicked.AddDynamic(this, &UDRSettingsWidget::OnGraphicsTabClicked);
    }
    if (Button_Controls)
    {
        Button_Controls->OnClicked.AddDynamic(this, &UDRSettingsWidget::OnControlsTabClicked);
    }

    // �ϴ� ��ư ���ε�
    if (Button_Apply)
    {
        Button_Apply->OnClicked.AddDynamic(this, &UDRSettingsWidget::OnApplyClicked);
    }
    if (Button_Back)
    {
        Button_Back->OnClicked.AddDynamic(this, &UDRSettingsWidget::OnBackClicked);
    }
    if (Button_MainMenu)
    {
        Button_MainMenu->OnClicked.AddDynamic(this, &UDRSettingsWidget::OnMainMenuClicked);
    }
    if (Button_QuitGame)
    {
        Button_QuitGame->OnClicked.AddDynamic(this, &UDRSettingsWidget::OnQuitGameClicked);
    }

    // �����̴� ���ε�
    if (Slider_MasterVolume)
    {
        Slider_MasterVolume->OnValueChanged.AddDynamic(this, &UDRSettingsWidget::OnMasterVolumeChanged);
    }
    if (Slider_BGMVolume)
    {
        Slider_BGMVolume->OnValueChanged.AddDynamic(this, &UDRSettingsWidget::OnBGMVolumeChanged);
    }
    if (Slider_SFXVolume)
    {
        Slider_SFXVolume->OnValueChanged.AddDynamic(this, &UDRSettingsWidget::OnSFXVolumeChanged);
    }
    if (Slider_VoiceVolume)
    {
        Slider_VoiceVolume->OnValueChanged.AddDynamic(this, &UDRSettingsWidget::OnVoiceVolumeChanged);
    }
    if (Slider_MouseSensitivity)
    {
        Slider_MouseSensitivity->OnValueChanged.AddDynamic(this, &UDRSettingsWidget::OnMouseSensitivityChanged);
    }

    // 콤보박스 바인딩
    if (ComboBox_Resolution)
    {
        ComboBox_Resolution->OnSelectionChanged.AddDynamic(this, &UDRSettingsWidget::OnResolutionChanged);
    }
    if (ComboBox_WindowMode)
    {
        ComboBox_WindowMode->OnSelectionChanged.AddDynamic(this, &UDRSettingsWidget::OnWindowModeChanged);
    }
    if (ComboBox_GraphicsQuality)
    {
        ComboBox_GraphicsQuality->OnSelectionChanged.AddDynamic(this, &UDRSettingsWidget::OnGraphicsQualityChanged);
    }

    // 해상도/창모드/그래픽 품질 옵션 채우기
    PopulateResolutionOptions();
    PopulateWindowModeOptions();
    PopulateGraphicsQualityOptions();

    // ���� ������ �ε�
    LoadCurrentSettings();
}

void UDRSettingsWidget::NativeDestruct()
{
    Super::NativeDestruct();
}

void UDRSettingsWidget::InitializeSettings()
{
    LoadCurrentSettings();
}

void UDRSettingsWidget::OpenSettings()
{
    SetVisibility(ESlateVisibility::Visible);
    LoadCurrentSettings();
}

void UDRSettingsWidget::CloseSettings()
{
    SetVisibility(ESlateVisibility::Collapsed);

    OnSettingsClosed();
}

void UDRSettingsWidget::LoadCurrentSettings()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UDRSettingsManager* Manager = GI->GetSubsystem<UDRSettingsManager>();
    if (!Manager) return;

    UDRGameUserSettings* Settings = Manager->GetSettings();
    if (!Settings) return;

    // �� �ε�
    PendingMasterVolume = Settings->MasterVolume;
    PendingBGMVolume = Settings->BGMVolume;
    PendingSFXVolume = Settings->SFXVolume;
    PendingVoiceVolume = Settings->VoiceVolume;
    PendingMouseSensitivity = Settings->MouseSensitivity;
    PendingResolution = Settings->GetScreenResolution();
    PendingWindowMode = Settings->GetFullscreenMode();

    if (Slider_MasterVolume)
    {
        Slider_MasterVolume->SetValue(PendingMasterVolume);
    }
    if (Text_MasterVolume)
    {
        Text_MasterVolume->SetText(GetPercentText(PendingMasterVolume));
    }
    if (Slider_BGMVolume)
    {
        Slider_BGMVolume->SetValue(PendingBGMVolume);
    }
    if (Text_BGMVolume)
    {
        Text_BGMVolume->SetText(GetPercentText(PendingBGMVolume));
    }
    if (Slider_SFXVolume)
    {
        Slider_SFXVolume->SetValue(PendingSFXVolume);
    }
    if (Text_SFXVolume)
    {
        Text_SFXVolume->SetText(GetPercentText(PendingSFXVolume));
    }
    // Voice�� 0~2 ������ �����̴� 0~1�� ��ȯ
    PendingVoiceVolume = Settings->VoiceVolume;
    if (Slider_VoiceVolume)
    {
        // 0.0~2.0 ���� �����̴� 0~1�� ��ȯ
        float SliderValue = PendingVoiceVolume / 2.0f;
        Slider_VoiceVolume->SetValue(SliderValue);
    }
    if (Text_VoiceVolume)
    {
        // 200%���� ǥ��
        int32 Percent = FMath::RoundToInt(PendingVoiceVolume * 100.0f);
        Text_VoiceVolume->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Percent)));
    }

    if (Slider_MouseSensitivity)
    {
        // ���� 0.1~5.0�� �����̴� 0~1�� ��ȯ
        float NormalizedValue = (PendingMouseSensitivity - 0.1f) / (5.0f - 0.1f);
        Slider_MouseSensitivity->SetValue(NormalizedValue);
    }
    if (Text_MouseSensitivity)
    {
        Text_MouseSensitivity->SetText(GetSensitivityText(PendingMouseSensitivity));
    }

    // �׷��� ���� �ε�
    PendingResolution = Settings->GetScreenResolution();
    PendingWindowMode = Settings->GetFullscreenMode();

    // �ػ� �޺��ڽ� ����
    if (ComboBox_Resolution)
    {
        FString ResolutionString = FString::Printf(TEXT("%d x %d"), PendingResolution.X, PendingResolution.Y);
        ComboBox_Resolution->SetSelectedOption(ResolutionString);
    }

    // 창모드 콤보박스 설정
    if (ComboBox_WindowMode)
    {
        FString WindowModeString;
        switch (PendingWindowMode)
        {
        case EWindowMode::Fullscreen:
            WindowModeString = TEXT("Fullscreen");
            break;
        case EWindowMode::WindowedFullscreen:
            WindowModeString = TEXT("WindowedFullscreen");
            break;
        case EWindowMode::Windowed:
            WindowModeString = TEXT("Windowed");
            break;
        default:
            WindowModeString = TEXT("Fullscreen");
            break;
        }
        ComboBox_WindowMode->SetSelectedOption(WindowModeString);
    }

    // 그래픽 품질 로드
    PendingGraphicsQuality = Settings->GetOverallScalabilityLevel();
    if (ComboBox_GraphicsQuality)
    {
        FString QualityString;
        switch (PendingGraphicsQuality)
        {
        case 0:
            QualityString = TEXT("Low");
            break;
        case 1:
            QualityString = TEXT("Medium");
            break;
        case 2:
            QualityString = TEXT("High");
            break;
        case 3:
            QualityString = TEXT("Epic");
            break;
        case 4:
            QualityString = TEXT("Cinematic");
            break;
        default:
            QualityString = TEXT("High");
            PendingGraphicsQuality = 2;
            break;
        }
        ComboBox_GraphicsQuality->SetSelectedOption(QualityString);
    }
}

void UDRSettingsWidget::PopulateResolutionOptions()
{
    if (!ComboBox_Resolution) return;

    ComboBox_Resolution->ClearOptions();
    SupportedResolutions.Empty();

    // ���� �ػ� ��������
    TArray<FIntPoint> Resolutions;
    UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);

    // �Ϲ����� �ػ󵵸� ���͸� (16:9, 16:10 ��)
    for (const FIntPoint& Res : Resolutions)
    {
        // �ּ� �ػ� ����
        if (Res.X >= 1280 && Res.Y >= 720)
        {
            SupportedResolutions.Add(Res);
            FString ResString = FString::Printf(TEXT("%d x %d"), Res.X, Res.Y);
            ComboBox_Resolution->AddOption(ResString);
        }
    }
}

void UDRSettingsWidget::PopulateWindowModeOptions()
{
    if (!ComboBox_WindowMode) return;

    ComboBox_WindowMode->ClearOptions();
    ComboBox_WindowMode->AddOption(TEXT("Fullscreen"));
    ComboBox_WindowMode->AddOption(TEXT("WindowedFullscreen"));
    ComboBox_WindowMode->AddOption(TEXT("Windowed"));
}

void UDRSettingsWidget::PopulateGraphicsQualityOptions()
{
    if (!ComboBox_GraphicsQuality) return;

    ComboBox_GraphicsQuality->ClearOptions();
    ComboBox_GraphicsQuality->AddOption(TEXT("Low"));
    ComboBox_GraphicsQuality->AddOption(TEXT("Medium"));
    ComboBox_GraphicsQuality->AddOption(TEXT("High"));
    ComboBox_GraphicsQuality->AddOption(TEXT("Epic"));
    ComboBox_GraphicsQuality->AddOption(TEXT("Cinematic"));
}

FText UDRSettingsWidget::GetPercentText(float Value) const
{
    int32 Percent = FMath::RoundToInt(Value * 100.0f);
    return FText::FromString(FString::Printf(TEXT("%d%%"), Percent));
}

FText UDRSettingsWidget::GetSensitivityText(float Value) const
{
    return FText::FromString(FString::Printf(TEXT("%.1f"), Value));
}

void UDRSettingsWidget::OnSoundTabClicked()
{
    if (SettingsSwitcher)
    {
        SettingsSwitcher->SetActiveWidgetIndex(0);
    }
}

void UDRSettingsWidget::OnGraphicsTabClicked()
{
    if (SettingsSwitcher)
    {
        SettingsSwitcher->SetActiveWidgetIndex(1);
    }
}

void UDRSettingsWidget::OnControlsTabClicked()
{
    if (SettingsSwitcher)
    {
        SettingsSwitcher->SetActiveWidgetIndex(2);
    }
}

void UDRSettingsWidget::OnApplyClicked()
{
    // Manager 가져오기
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UDRSettingsManager* Manager = GI->GetSubsystem<UDRSettingsManager>();
    if (!Manager) return;

    // Manager 통해 설정 적용
    Manager->SetMasterVolume(PendingMasterVolume);
    Manager->SetBGMVolume(PendingBGMVolume);
    Manager->SetSFXVolume(PendingSFXVolume);
    Manager->SetVoiceVolume(PendingVoiceVolume);
    Manager->SetMouseSensitivity(PendingMouseSensitivity);
    Manager->SetScreenResolution(PendingResolution);
    Manager->SetWindowMode(PendingWindowMode);
    Manager->SetGraphicsQuality(PendingGraphicsQuality);

    // 모든 설정 적용 및 저장
    Manager->ApplyAndSaveAllSettings();
}

void UDRSettingsWidget::OnBackClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(PC))
        {
            DRPC->CloseSettingsMenu();
            return;
        }
    }

    // ���� �ݱ�
    CloseSettings();
}

void UDRSettingsWidget::OnMainMenuClicked()
{
    // ���� ����
    if (UDRGameUserSettings* Settings = UDRGameUserSettings::GetDRGameUserSettings())
    {
        Settings->SaveSettings();
    }

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    // ���� ����
    UGameInstance* GameInstance = PC->GetGameInstance();
    if (GameInstance)
    {
        if (UMultiplayerSessionsSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>())
        {
            SessionSubsystem->LeaveServer();
        }
    }
}

void UDRSettingsWidget::OnQuitGameClicked()
{
    // ���� ����
    if (UDRGameUserSettings* Settings = UDRGameUserSettings::GetDRGameUserSettings())
    {
        Settings->SaveSettings();
    }

    // ���� ����
    APlayerController* PC = GetOwningPlayer();
    if (PC)
    {
        UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
    }
}

void UDRSettingsWidget::OnMasterVolumeChanged(float Value)
{
    PendingMasterVolume = Value;
    if (Text_MasterVolume)
    {
        Text_MasterVolume->SetText(GetPercentText(Value));
    }
}

void UDRSettingsWidget::OnBGMVolumeChanged(float Value)
{
    PendingBGMVolume = Value;
    if (Text_BGMVolume)
    {
        Text_BGMVolume->SetText(GetPercentText(Value));
    }
}

void UDRSettingsWidget::OnSFXVolumeChanged(float Value)
{
    PendingSFXVolume = Value;
    if (Text_SFXVolume)
    {
        Text_SFXVolume->SetText(GetPercentText(Value));
    }
}

void UDRSettingsWidget::OnVoiceVolumeChanged(float Value)
{
    // �����̴� 0~1 ���� ���� ���� 0~2�� ��ȯ
    PendingVoiceVolume = Value * 2.0f;
    if (Text_VoiceVolume)
    {
        // 200%���� ǥ��
        int32 Percent = FMath::RoundToInt(PendingVoiceVolume * 100.0f);
        Text_VoiceVolume->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Percent)));
    }
}

void UDRSettingsWidget::OnMouseSensitivityChanged(float Value)
{
    // �����̴� 0~1�� ���� 0.1~5.0���� ��ȯ
    PendingMouseSensitivity = FMath::Lerp(0.1f, 5.0f, Value);
    if (Text_MouseSensitivity)
    {
        Text_MouseSensitivity->SetText(GetSensitivityText(PendingMouseSensitivity));
    }
}

void UDRSettingsWidget::OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    // "1920 x 1080" 형식에서 값을 파싱
    TArray<FString> Parts;
    SelectedItem.ParseIntoArray(Parts, TEXT(" x "));

    if (Parts.Num() == 2)
    {
        int32 ParsedX = FCString::Atoi(*Parts[0]);
        int32 ParsedY = FCString::Atoi(*Parts[1]);

        // 유효성 검사: 최소 해상도 체크
        if (ParsedX >= 1280 && ParsedY >= 720)
        {
            PendingResolution.X = ParsedX;
            PendingResolution.Y = ParsedY;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("OnResolutionChanged: Invalid resolution parsed - %d x %d"), ParsedX, ParsedY);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("OnResolutionChanged: Failed to parse resolution string - %s"), *SelectedItem);
    }
}

void UDRSettingsWidget::OnWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectedItem == TEXT("Fullscreen"))
    {
        PendingWindowMode = EWindowMode::Fullscreen;
    }
    else if (SelectedItem == TEXT("WindowedFullscreen"))
    {
        PendingWindowMode = EWindowMode::WindowedFullscreen;
    }
    else if (SelectedItem == TEXT("Windowed"))
    {
        PendingWindowMode = EWindowMode::Windowed;
    }
}

void UDRSettingsWidget::OnGraphicsQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (SelectedItem == TEXT("Low"))
    {
        PendingGraphicsQuality = 0;
    }
    else if (SelectedItem == TEXT("Medium"))
    {
        PendingGraphicsQuality = 1;
    }
    else if (SelectedItem == TEXT("High"))
    {
        PendingGraphicsQuality = 2;
    }
    else if (SelectedItem == TEXT("Epic"))
    {
        PendingGraphicsQuality = 3;
    }
    else if (SelectedItem == TEXT("Cinematic"))
    {
        PendingGraphicsQuality = 4;
    }
}
