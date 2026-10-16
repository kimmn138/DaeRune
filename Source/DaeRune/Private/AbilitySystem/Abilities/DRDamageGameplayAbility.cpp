// Copyright DaeRune


#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "DaeRune/DRLogChannels.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "DaeRune/Public/DRGameplayTags.h"

void UDRDamageGameplayAbility::CauseDamage(AActor* TargetActor)
{
	FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, 1.f);
	const float ScaledDamage = GetUpgradedDamage();
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(DamageSpecHandle, DamageType, ScaledDamage);
	GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor));
}

FDamageEffectParams UDRDamageGameplayAbility::MakeDamageEffectParamsFromClassDefaults(AActor* TargetActor) const
{
	FDamageEffectParams Params; 
	Params.WorldContextObject = GetAvatarActorFromActorInfo();
	Params.DamageGameplayEffectClass = DamageEffectClass;
	Params.SourceAbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	Params.TargetAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	Params.BaseDamage = GetUpgradedDamage();
	Params.AbilityLevel = GetAbilityLevel();
	Params.DamageType = DamageType;
	Params.DebuffChance = DebuffChance;
	Params.DebuffDamage = DebuffDamage;
	Params.DebuffDuration = DebuffDuration;
	Params.DeathImpulseMagnitude = DeathImpulseMagnitude;
	Params.KnockbackForceMagnitude = KnockbackForceMagnitude;
	Params.KnockbackChance = KnockbackChance;
	Params.SourceAbilityTags = GetAssetTags();
	if (IsValid(TargetActor))
	{
		FRotator Rotation = (TargetActor->GetActorLocation() - GetAvatarActorFromActorInfo()->GetActorLocation()).Rotation();
		Rotation.Pitch = 45.f;
		const FVector ToTarget = Rotation.Vector();
		Params.DeathImpulse = ToTarget * DeathImpulseMagnitude;
		Params.KnockbackForce = ToTarget * KnockbackForceMagnitude;
	}
	return Params;
}

float UDRDamageGameplayAbility::GetDamageAtLevel() const
{
	return GetUpgradedDamage();
}

float UDRDamageGameplayAbility::GetUpgradedDamage() const
{
	const float Upgraded = GetUpgradedFloat(EDRUpgradeStat::SkillDamage, Damage.GetValueAtLevel(GetAbilityLevel()));

	// ★Plan5(물대포칩)★ 런타임 배율(물대포 가압 스택 등)을 마지막에 곱한다.
	// 곱셈이라 SkillDamage 와 순서를 다퉈도 결과가 같고, 기본 구현이 1.0 이라
	// 이 훅을 오버라이드하지 않은 어빌리티는 수치가 그대로다.
	return Upgraded * GetRuntimeDamageMultiplier();
}

FTaggedMontage UDRDamageGameplayAbility::GetRandomTaggedMontageFromArray(const TArray<FTaggedMontage>& TaggedMontages) const
{
	if (TaggedMontages.Num() > 0)
	{
		const int32 Selection = FMath::RandRange(0, TaggedMontages.Num() - 1);
		return TaggedMontages[Selection];
	}

	return FTaggedMontage();
}
