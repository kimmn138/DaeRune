// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "DRSettingsTypes.generated.h"

/**
 * 설정 탭 종류
 */
UENUM(BlueprintType)
enum class EDRSettingsTab : uint8
{
	Gameplay,
	Graphics,
	Audio,
	Controls
};

/**
 * 설정 컨트롤 타입
 */
UENUM(BlueprintType)
enum class EDRSettingsControlType : uint8
{
	Dropdown,
	Toggle,
	Slider,
	KeyBind
};

/**
 * 설정 값 타입
 */
UENUM(BlueprintType)
enum class EDRSettingsValueType : uint8
{
	Bool,
	Int,
	Float,
	Name,
	String,
	Resolution,
	Key
};

/**
 * 설정 적용 모드
 */
UENUM(BlueprintType)
enum class EDRSettingsApplyMode : uint8
{
	Instant,
	RequiresApply
};

/**
 * 드롭다운 옵션 하나를 표현하는 구조체
 */
USTRUCT(BlueprintType)
struct FDRSettingsOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FName OptionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText DisplayText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 IntValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float FloatValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FString StringValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 ResolutionX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 ResolutionY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FKey KeyValue;
};

/**
 * 현재값, 대기값, 기본값을 담는 구조체
 */
USTRUCT(BlueprintType)
struct FDRSettingsValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	EDRSettingsValueType ValueType = EDRSettingsValueType::Float;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool BoolValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 IntValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float FloatValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FName NameValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FString StringValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 ResolutionX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 ResolutionY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FKey KeyValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 SelectedIndex = 0;
};

/**
 * 설정 항목 하나의 UI 정의
 */
USTRUCT(BlueprintType)
struct FDRSettingDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FName SettingId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	EDRSettingsTab Tab = EDRSettingsTab::Graphics;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText LabelText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	EDRSettingsControlType ControlType = EDRSettingsControlType::Dropdown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	EDRSettingsValueType ValueType = EDRSettingsValueType::Float;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FDRSettingsValue DefaultValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TArray<FDRSettingsOption> Options;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float MinValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float MaxValue = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float StepValue = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FText SuffixText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bShowSeparator = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	EDRSettingsApplyMode ApplyMode = EDRSettingsApplyMode::RequiresApply;
};
