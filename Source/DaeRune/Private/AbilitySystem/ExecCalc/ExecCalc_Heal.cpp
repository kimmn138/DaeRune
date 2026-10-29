// Copyright DaeRune


#include "AbilitySystem/ExecCalc/ExecCalc_Heal.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "DRGameplayTags.h"

void UExecCalc_Heal::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// Get Heal Set by Caller Magnitude
	const float Heal = Spec.GetSetByCallerMagnitude(FDRGameplayTags::Get().Heal, false, 0.0f);

	const FGameplayModifierEvaluatedData EvaluatedData(UDRAttributeSet::GetIncomingHealingAttribute(), EGameplayModOp::Additive, Heal);
	OutExecutionOutput.AddOutputModifier(EvaluatedData);
}
