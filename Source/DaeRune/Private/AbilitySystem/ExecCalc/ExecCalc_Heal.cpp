// Copyright DaeRune


#include "AbilitySystem/ExecCalc/ExecCalc_Heal.h"
#include "AbilitySystem/DRAttributeSet.h"

UExecCalc_Heal::UExecCalc_Heal()
{
}

void UExecCalc_Heal::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = SourceTags;
	EvaluationParameters.TargetTags = TargetTags;

	// Get Heal Set by Caller Magnitude
	float Heal = Spec.GetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Heal")), false, 0.0f);

	const FGameplayModifierEvaluatedData EvaluatedData(UDRAttributeSet::GetIncomingHealingAttribute(), EGameplayModOp::Additive, Heal);
	OutExecutionOutput.AddOutputModifier(EvaluatedData);
}
