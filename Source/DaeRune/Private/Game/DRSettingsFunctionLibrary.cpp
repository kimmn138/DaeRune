// Copyright DaeRune


#include "Game/DRSettingsFunctionLibrary.h"

FDRSettingsValue UDRSettingsFunctionLibrary::MakeBoolValue(bool InBool)
{
	FDRSettingsValue Value;
	Value.ValueType = EDRSettingsValueType::Bool;
	Value.BoolValue = InBool;
	return Value;
}

FDRSettingsValue UDRSettingsFunctionLibrary::MakeFloatValue(float InFloat)
{
	FDRSettingsValue Value;
	Value.ValueType = EDRSettingsValueType::Float;
	Value.FloatValue = InFloat;
	return Value;
}

FDRSettingsValue UDRSettingsFunctionLibrary::MakeIntValue(int32 InInt, int32 SelectedIndex)
{
	FDRSettingsValue Value;
	Value.ValueType = EDRSettingsValueType::Int;
	Value.IntValue = InInt;
	Value.SelectedIndex = SelectedIndex;
	return Value;
}

FDRSettingsValue UDRSettingsFunctionLibrary::MakeNameValue(FName InName, int32 SelectedIndex)
{
	FDRSettingsValue Value;
	Value.ValueType = EDRSettingsValueType::Name;
	Value.NameValue = InName;
	Value.SelectedIndex = SelectedIndex;
	return Value;
}

FDRSettingsValue UDRSettingsFunctionLibrary::MakeResolutionValue(int32 X, int32 Y, int32 SelectedIndex)
{
	FDRSettingsValue Value;
	Value.ValueType = EDRSettingsValueType::Resolution;
	Value.ResolutionX = X;
	Value.ResolutionY = Y;
	Value.SelectedIndex = SelectedIndex;
	return Value;
}

bool UDRSettingsFunctionLibrary::IsSettingsValueEqual(const FDRSettingsValue& A, const FDRSettingsValue& B)
{
	if (A.ValueType != B.ValueType)
	{
		return false;
	}

	switch (A.ValueType)
	{
	case EDRSettingsValueType::Bool:
		return A.BoolValue == B.BoolValue;
	case EDRSettingsValueType::Int:
		return A.IntValue == B.IntValue;
	case EDRSettingsValueType::Float:
		return FMath::IsNearlyEqual(A.FloatValue, B.FloatValue, 0.01f);
	case EDRSettingsValueType::Name:
		return A.NameValue == B.NameValue;
	case EDRSettingsValueType::String:
		return A.StringValue == B.StringValue;
	case EDRSettingsValueType::Resolution:
		return A.ResolutionX == B.ResolutionX && A.ResolutionY == B.ResolutionY;
	case EDRSettingsValueType::Key:
		return A.KeyValue == B.KeyValue;
	default:
		return false;
	}
}

float UDRSettingsFunctionLibrary::ClampAndSnapFloat(float Value, float Min, float Max, float Step)
{
	float Clamped = FMath::Clamp(Value, Min, Max);

	if (Step <= 0.f)
	{
		return Clamped;
	}

	int32 StepCount = FMath::RoundToInt((Clamped - Min) / Step);
	return Min + StepCount * Step;
}

int32 UDRSettingsFunctionLibrary::GetOptionIndexByName(const TArray<FDRSettingsOption>& Options, FName OptionId)
{
	for (int32 i = 0; i < Options.Num(); ++i)
	{
		if (Options[i].OptionId == OptionId)
		{
			return i;
		}
	}
	return 0;
}
