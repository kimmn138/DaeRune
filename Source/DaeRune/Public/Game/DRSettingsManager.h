// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Game/DRSettingsTypes.h"
#include "DRSettingsManager.generated.h"

class UDRGameUserSettings;
class USoundMix;
class USoundClass;
class UUserWidget;

// 설정 로드 완료 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsLoaded);

// 설정 적용 완료 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsApplied);

// Pending 값 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPendingValueChanged, FName, SettingId, FDRSettingsValue, NewValue);

// Pending 변경사항 유무 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHasPendingChangesChanged, bool, bHasPendingChanges);

// 탭 리셋 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSettingsReset, EDRSettingsTab, Tab);

// 언어 변경 델리게이트 (CultureCode: "ko", "en")
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLanguageChanged, const FString&, CultureCode);

/**
 * 설정 관리 매니저 - 기존 오디오/그래픽 적용 로직 유지 + 데이터 주도 관리 레이어
 */
UCLASS()
class DAERUNE_API UDRSettingsManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()


public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category = "Settings")
    UDRGameUserSettings* GetSettings() const;

    // ========== 기존 함수 (유지) ==========

    // 모든 설정 적용 및 저장
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplyAndSaveAllSettings();

    // 해상도/창모드 적용
    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void ApplyResolutionSettings();

    // 해상도 외 설정 적용
    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void ApplyNonResolutionSettings();

    // 오디오 볼륨 적용
    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void ApplyAudioSettings();

    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void SetMasterVolume(float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void SetBGMVolume(float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "Settings|Audio")
    void SetSFXVolume(float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
    void SetMouseSensitivity(float NewSensitivity);

    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void SetScreenResolution(FIntPoint NewResolution);

    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void SetWindowMode(EWindowMode::Type NewMode);

    // Scalability 설정 (0: Low, 1: Medium, 2: High, 3: Epic, 4: Cinematic)
    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void SetGraphicsQuality(int32 QualityLevel);

    UFUNCTION(BlueprintCallable, Category = "Settings|Graphics")
    void ApplyGraphicsQualitySettings();

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ResetToDefaults();

    UFUNCTION(BlueprintPure, Category = "Settings")
    TArray<FIntPoint> GetSupportedResolutions() const;

    // ========== 데이터 주도 관리 레이어 ==========

    /** 설정 시스템 초기화 (BuildDefinitions → BuildDefaultValues → LoadFromGameUserSettings) */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    void InitSettings();

    /** 설정 정의 배열 구축 */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    void BuildDefinitions();

    /** DefaultValues 맵 구축 (Definitions 기반) */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    void BuildDefaultValues();

    /** GameUserSettings에서 CurrentValues로 로드 */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    void LoadFromGameUserSettings();

    /** CurrentValues를 GameUserSettings에 저장 */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    void SaveToGameUserSettings();

    /** 탭별 설정 정의 가져오기 (Order로 정렬) */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    TArray<FDRSettingDefinition> GetDefinitionsByTab(EDRSettingsTab Tab) const;

    /** Pending 값 가져오기 */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    FDRSettingsValue GetPendingValue(FName SettingId) const;

    /** Pending 값 설정 (Instant 모드면 즉시 적용) */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    void SetPendingValue(FName SettingId, const FDRSettingsValue& NewValue);

    /** 모든 Pending 변경사항 적용 */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    void ApplyPendingSettings();

    /** 단일 설정 적용 (실제 엔진/오디오 설정 반영) */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    void ApplySingleSetting(FName SettingId, const FDRSettingsValue& Value);

    /** 단일 설정 커밋 (Current = Pending, Dirty 제거) */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    void CommitSingleSetting(FName SettingId);

    /** 탭 설정을 기본값으로 리셋 */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    void ResetTabToDefault(EDRSettingsTab Tab, bool bApplyImmediately = false);

    /** Pending 변경사항 폐기 (Current로 복원) */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    void DiscardPendingChanges();

    /** 특정 설정이 Dirty 상태인지 확인 */
    UFUNCTION(BlueprintPure, Category = "Settings|DataDriven")
    bool IsSettingDirty(FName SettingId) const;

    /** ID로 설정 정의 가져오기 */
    UFUNCTION(BlueprintCallable, Category = "Settings|DataDriven")
    bool GetDefinitionById(FName SettingId, FDRSettingDefinition& OutDefinition) const;

    // ========== 이벤트 디스패처 ==========

    UPROPERTY(BlueprintAssignable, Category = "Settings")
    FOnSettingsApplied OnSettingsApplied;

    UPROPERTY(BlueprintAssignable, Category = "Settings|DataDriven")
    FOnSettingsLoaded OnSettingsLoaded;

    UPROPERTY(BlueprintAssignable, Category = "Settings|DataDriven")
    FOnPendingValueChanged OnPendingValueChanged;

    UPROPERTY(BlueprintAssignable, Category = "Settings|DataDriven")
    FOnHasPendingChangesChanged OnHasPendingChangesChanged;

    UPROPERTY(BlueprintAssignable, Category = "Settings|DataDriven")
    FOnSettingsReset OnSettingsReset;

    /** 언어가 실제로 적용된 직후 발행. UI는 이 시점에 LOCTEXT 기반 텍스트를 다시 갱신해야 한다. */
    UPROPERTY(BlueprintAssignable, Category = "Settings|Localization")
    FOnLanguageChanged OnLanguageChanged;

    /** Gameplay.Language OptionId(FName: "Korean", "English") → 컬처 코드(FString: "ko", "en"). 없으면 빈 문자열. */
    static FString LanguageOptionIdToCulture(FName OptionId);

    /** 컬처 코드 → OptionId 역변환. 없으면 NAME_None. */
    static FName CultureToLanguageOptionId(const FString& CultureCode);

    // ========== 데이터 접근 ==========

    UPROPERTY(BlueprintReadOnly, Category = "Settings|DataDriven")
    bool bHasPendingChanges = false;

    UPROPERTY(BlueprintReadOnly, Category = "Settings|DataDriven")
    bool bLoaded = false;

    /** 현재 열려있는 드롭다운 위젯 (새 드롭다운 열 때 이전 것을 닫기 위해 추적) */
    UPROPERTY(BlueprintReadWrite, Transient, Category = "Settings|UI")
    TObjectPtr<UUserWidget> ActiveDropdownWidget;

private:
    // SoundMix 볼륨 적용 함수
    void ApplySoundMixToWorld(UWorld* World);

    // 내부 헬퍼: Pending 변경 상태 갱신
    void UpdateHasPendingChanges();

    // 오디오 리소스
    UPROPERTY()
    TObjectPtr<USoundMix> GameSoundMix;

    UPROPERTY()
    TObjectPtr<USoundClass> MasterSoundClass;

    UPROPERTY()
    TObjectPtr<USoundClass> BGMSoundClass;

    UPROPERTY()
    TObjectPtr<USoundClass> SFXSoundClass;

    // 데이터 주도 설정 변수
    UPROPERTY()
    TArray<FDRSettingDefinition> Definitions;

    UPROPERTY()
    TMap<FName, FDRSettingsValue> CurrentValues;

    UPROPERTY()
    TMap<FName, FDRSettingsValue> PendingValues;

    UPROPERTY()
    TMap<FName, FDRSettingsValue> DefaultValues;

    UPROPERTY()
    TArray<FName> DirtySettingIds;

    bool bApplying = false;
};
