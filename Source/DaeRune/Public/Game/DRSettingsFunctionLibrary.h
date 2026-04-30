// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Game/DRSettingsTypes.h"
#include "DRSettingsFunctionLibrary.generated.h"

/**
 * 설정 값 생성/비교 헬퍼 함수 라이브러리
 */
UCLASS()
class DAERUNE_API UDRSettingsFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Settings|Helpers")
	static FDRSettingsValue MakeBoolValue(bool InBool);

	UFUNCTION(BlueprintPure, Category = "Settings|Helpers")
	static FDRSettingsValue MakeFloatValue(float InFloat);

	UFUNCTION(BlueprintPure, Category = "Settings|Helpers")
	static FDRSettingsValue MakeIntValue(int32 InInt, int32 SelectedIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Settings|Helpers")
	static FDRSettingsValue MakeNameValue(FName InName, int32 SelectedIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Settings|Helpers")
	static FDRSettingsValue MakeResolutionValue(int32 X, int32 Y, int32 SelectedIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Settings|Helpers")
	static bool IsSettingsValueEqual(const FDRSettingsValue& A, const FDRSettingsValue& B);

	UFUNCTION(BlueprintPure, Category = "Settings|Helpers")
	static float ClampAndSnapFloat(float Value, float Min, float Max, float Step);

	UFUNCTION(BlueprintPure, Category = "Settings|Helpers")
	static int32 GetOptionIndexByName(const TArray<FDRSettingsOption>& Options, FName OptionId);
};
