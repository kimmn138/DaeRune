// Copyright DaeRune


#include "AbilitySystem/ExecCalc/ExecCalc_Damage.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAttributeSet.h"

UExecCalc_Damage::UExecCalc_Damage()
{
}

void UExecCalc_Damage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// Get Damage Set by Caller Magnitude
	float Damage = 0.0f;
	for (const FGameplayTag& DamageTypeTag : FDRGameplayTags::Get().DamageTypeTags)
	{
		float DamageTypeValue = Spec.GetSetByCallerMagnitude(DamageTypeTag, false);
		Damage += DamageTypeValue;
	}

	const FGameplayModifierEvaluatedData EvaluatedData(UDRAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, Damage);
	OutExecutionOutput.AddOutputModifier(EvaluatedData);
}
