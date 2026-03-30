// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRSettingsWidget.generated.h"

class UButton;
class UWidgetSwitcher;
class USlider;
class UTextBlock;
class UComboBoxString;

/**
 * ���� �޴� ������ ���̽� Ŭ����
 */
UCLASS()
class DAERUNE_API UDRSettingsWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
    // ���� �޴� �ʱ�ȭ
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void InitializeSettings();

    // ���� �޴� ����/�ݱ�
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void OpenSettings();

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void CloseSettings();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ========== �� ��ư�� ==========

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Sound;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Graphics;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Controls;

    // ========== �г� ��ȯ�� ==========

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> SettingsSwitcher;

    // ========== �ϴ� ��ư�� ==========

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Apply;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Back;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_MainMenu;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_QuitGame;

    // ========== ���� ���� ==========

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_MasterVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_MasterVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_BGMVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_BGMVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_SFXVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_SFXVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_VoiceVolume;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_VoiceVolume;

    // ========== 그래픽 설정 ==========

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> ComboBox_Resolution;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> ComboBox_WindowMode;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> ComboBox_GraphicsQuality;

    // ========== ���� ���� ==========

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_MouseSensitivity;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_MouseSensitivity;

    // ========== �� ��ư �ݹ� ==========

    UFUNCTION()
    void OnSoundTabClicked();

    UFUNCTION()
    void OnGraphicsTabClicked();

    UFUNCTION()
    void OnControlsTabClicked();

    // ========== �ϴ� ��ư �ݹ� ==========

    UFUNCTION()
    void OnApplyClicked();

    UFUNCTION()
    void OnBackClicked();

    UFUNCTION()
    void OnMainMenuClicked();

    UFUNCTION()
    void OnQuitGameClicked();

    // ========== �����̴� �ݹ� ==========

    UFUNCTION()
    void OnMasterVolumeChanged(float Value);

    UFUNCTION()
    void OnBGMVolumeChanged(float Value);

    UFUNCTION()
    void OnSFXVolumeChanged(float Value);

    UFUNCTION()
    void OnVoiceVolumeChanged(float Value);

    UFUNCTION()
    void OnMouseSensitivityChanged(float Value);

    // ========== �޺��ڽ� �ݹ� ==========

    UFUNCTION()
    void OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnGraphicsQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    // ========== �ڷΰ��� �̺�Ʈ ==========

    UFUNCTION(BlueprintImplementableEvent, Category = "Settings")
    void OnSettingsClosed();

private:
    // UI�� ���� ������ �ε�
    void LoadCurrentSettings();

    // �ػ� ��� ä���
    void PopulateResolutionOptions();

    // 창모드 옵션 채우기
    void PopulateWindowModeOptions();

    // 그래픽 품질 옵션 채우기
    void PopulateGraphicsQualityOptions();

    // �����̴� ���� �ۼ�Ʈ �ؽ�Ʈ�� ��ȯ
    FText GetPercentText(float Value) const;

    // �����̴� ���� ���� �ؽ�Ʈ�� ��ȯ (0.1 ~ 5.0)
    FText GetSensitivityText(float Value) const;

    // 임시 설정 저장용
    float PendingMasterVolume;
    float PendingBGMVolume;
    float PendingSFXVolume;
    float PendingVoiceVolume;
    float PendingMouseSensitivity;
    FIntPoint PendingResolution;
    EWindowMode::Type PendingWindowMode;
    int32 PendingGraphicsQuality;

    // ���� �ػ� ���
    TArray<FIntPoint> SupportedResolutions;
};
