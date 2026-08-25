// Copyright DaeRune


#include "AbilitySystem/ExecCalc/ExecCalc_Damage.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/DRCleanserSiteAttributeSet.h"

UExecCalc_Damage::UExecCalc_Damage()
{
}

void UExecCalc_Damage::DetermineDebuff(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FGameplayEffectSpec& Spec) const
{
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();

	for (TTuple<FGameplayTag, FGameplayTag> Pair : GameplayTags.DamageTypesToDebuffs)
	{
		const FGameplayTag& DamageType = Pair.Key;
		const FGameplayTag& DebuffType = Pair.Value;
		const float TypeDamage = Spec.GetSetByCallerMagnitude(DamageType, false, -1.f);
		if (TypeDamage > -.5f) // .5 padding for floating point [im]precision
		{
			// Determine if there was a successful debuff
			const float SourceDebuffChance = Spec.GetSetByCallerMagnitude(GameplayTags.Debuff_Chance, false, -1.f);

			const bool bDebuff = FMath::RandRange(1, 100) <= SourceDebuffChance;
			if (bDebuff)
			{
				FGameplayEffectContextHandle ContextHandle = Spec.GetContext(); 

				UDRAbilitySystemLibrary::SetIsSuccessfulDebuff(ContextHandle, true);
				UDRAbilitySystemLibrary::SetDamageType(ContextHandle, DamageType);

				const float DebuffDamage = Spec.GetSetByCallerMagnitude(GameplayTags.Debuff_Damage, false, -1.f);
				const float DebuffDuration = Spec.GetSetByCallerMagnitude(GameplayTags.Debuff_Duration, false, -1.f);

				UDRAbilitySystemLibrary::SetDebuffDamage(ContextHandle, DebuffDamage);
				UDRAbilitySystemLibrary::SetDebuffDuration(ContextHandle, DebuffDuration);
			}
		}
	}
}

void UExecCalc_Damage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = SourceTags;
	EvaluationParameters.TargetTags = TargetTags;

	// DebuffMore actions
	DetermineDebuff(ExecutionParams, Spec);

	// Get Damage Set by Caller Magnitude
	float Damage = 0.0f;
	for (const FGameplayTag& DamageTypeTag : FDRGameplayTags::Get().DamageTypeTags)
	{
		float DamageTypeValue = Spec.GetSetByCallerMagnitude(DamageTypeTag, false);
		Damage += DamageTypeValue;
	}

	// Elite Roar Aura: enemies with Buff.Elite.Roar deal 20% more damage
	const UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	if (SourceASC && SourceASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Buff_Elite_Roar))
	{
		Damage *= 1.2f;
	}

	// 타겟의 AttributeSet 타입에 따라 올바른 IncomingDamage Attribute 사용
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	FGameplayAttribute IncomingDamageAttribute;

	if (TargetASC)
	{
		// 클렌저 사이트 AttributeSet 체크
		const bool bTargetIsCleanserSite = TargetASC->HasAttributeSetForAttribute(UDRCleanserSiteAttributeSet::GetIncomingDamageAttribute());
		if (bTargetIsCleanserSite)
		{
			IncomingDamageAttribute = UDRCleanserSiteAttributeSet::GetIncomingDamageAttribute();
		}
		// 일반 캐릭터 AttributeSet
		else
		{
			IncomingDamageAttribute = UDRAttributeSet::GetIncomingDamageAttribute();
		}

		// New Phase1 적 → 플레이어/적 대미지 0.7배 (클렌저 사이트 제외)
		if (!bTargetIsCleanserSite
			&& SourceASC
			&& SourceASC->HasMatchingGameplayTag(FDRGameplayTags::Get().State_Enemy_Phase1))
		{
			Damage *= 0.7f;
		}

		const FGameplayModifierEvaluatedData EvaluatedData(IncomingDamageAttribute, EGameplayModOp::Additive, Damage);
		OutExecutionOutput.AddOutputModifier(EvaluatedData);
	}
}
