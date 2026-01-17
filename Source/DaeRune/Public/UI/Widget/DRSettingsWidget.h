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
 * 설정 메뉴 위젯의 베이스 클래스
 */
UCLASS()
class DAERUNE_API UDRSettingsWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
    // 설정 메뉴 초기화
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void InitializeSettings();

    // 설정 메뉴 열기/닫기
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void OpenSettings();

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void CloseSettings();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ========== 탭 버튼들 ==========

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Sound;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Graphics;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Controls;

    // ========== 패널 전환기 ==========

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> SettingsSwitcher;

    // ========== 하단 버튼들 ==========

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Apply;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Back;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_MainMenu;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_QuitGame;

    // ========== 사운드 설정 ==========

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

    // ========== 조작 설정 ==========

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> Slider_MouseSensitivity;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_MouseSensitivity;

    // ========== 탭 버튼 콜백 ==========

    UFUNCTION()
    void OnSoundTabClicked();

    UFUNCTION()
    void OnGraphicsTabClicked();

    UFUNCTION()
    void OnControlsTabClicked();

    // ========== 하단 버튼 콜백 ==========

    UFUNCTION()
    void OnApplyClicked();

    UFUNCTION()
    void OnBackClicked();

    UFUNCTION()
    void OnMainMenuClicked();

    UFUNCTION()
    void OnQuitGameClicked();

    // ========== 슬라이더 콜백 ==========

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

    // ========== 콤보박스 콜백 ==========

    UFUNCTION()
    void OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    // ========== 뒤로가기 이벤트 ==========

    UFUNCTION(BlueprintImplementableEvent, Category = "Settings")
    void OnSettingsClosed();

private:
    // UI에 현재 설정값 로드
    void LoadCurrentSettings();

    // 해상도 목록 채우기
    void PopulateResolutionOptions();

    // 창모드 목록 채우기
    void PopulateWindowModeOptions();

    // 슬라이더 값을 퍼센트 텍스트로 변환
    FText GetPercentText(float Value) const;

    // 슬라이더 값을 감도 텍스트로 변환 (0.1 ~ 5.0)
    FText GetSensitivityText(float Value) const;

    // 임시 저장 변수들
    float PendingMasterVolume;
    float PendingBGMVolume;
    float PendingSFXVolume;
    float PendingVoiceVolume;
    float PendingMouseSensitivity;
    FIntPoint PendingResolution;
    EWindowMode::Type PendingWindowMode;

    // 지원 해상도 목록
    TArray<FIntPoint> SupportedResolutions;
};
