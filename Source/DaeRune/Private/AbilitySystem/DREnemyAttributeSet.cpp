// Copyright DaeRune


#include "AbilitySystem/DREnemyAttributeSet.h"
#include "Interaction/CombatInterface.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "DRGameplayTags.h"
#include "GameFramework/Character.h"

void UDREnemyAttributeSet::HandleIncomingDamage(const FEffectProperties& Props)
{
	const float LocalIncomingDamage = GetIncomingDamage();
	SetIncomingDamage(0.f);
	if (LocalIncomingDamage > 0.f)
	{
		// 데미지가 발생하면 전투 상태 진입
		NotifyEnterCombat(Props);

		const float NewHealth = GetHealth() - LocalIncomingDamage;
		SetHealth(FMath::Clamp(NewHealth, 0.f, GetMaxHealth()));

		const bool bFatal = NewHealth <= 0.f;
		if (bFatal)
		{
			ICombatInterface* CombatInterface = Cast<ICombatInterface>(Props.TargetAvatarActor);
			if (CombatInterface)
			{
				FVector Impulse = UDRAbilitySystemLibrary::GetDeathImpulse(Props.EffectContextHandle);
				CombatInterface->Die(UDRAbilitySystemLibrary::GetDeathImpulse(Props.EffectContextHandle));
			}
		}
		else
		{
			if (Props.TargetCharacter->Implements<UCombatInterface>() && !ICombatInterface::Execute_IsBeingShocked(Props.TargetCharacter))
			{
				FGameplayTagContainer TagContainer;
				TagContainer.AddTag(FDRGameplayTags::Get().Effects_HitReact);
				Props.TargetASC->TryActivateAbilitiesByTag(TagContainer);
			}

			const FVector& KnockbackForce = UDRAbilitySystemLibrary::GetKnockbackForce(Props.EffectContextHandle);
			if (!KnockbackForce.IsNearlyZero(1.f))
			{
				Props.TargetCharacter->LaunchCharacter(KnockbackForce, true, true);
			}
		}

		ShowFloatingText(Props, LocalIncomingDamage);
		if (UDRAbilitySystemLibrary::IsSuccessfulDebuff(Props.EffectContextHandle))
		{
			Debuff(Props);
		}
	}
}

void UDREnemyAttributeSet::HandleIncomingHealing(const FEffectProperties& Props)
{
	const float LocalIncomingHealing = GetIncomingHealing();
	SetIncomingHealing(0.f);

	if (LocalIncomingHealing > 0.f)
	{
		const float NewHealth = GetHealth() + LocalIncomingHealing;
		SetHealth(FMath::Clamp(NewHealth, 0.f, GetMaxHealth()));
	}
}
