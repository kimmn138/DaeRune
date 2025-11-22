// Copyright DaeRune


#include "AbilitySystem/DREnemyAttributeSet.h"
#include "Interaction/CombatInterface.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "DRGameplayTags.h"
#include "GameFramework/Character.h"
#include "Character/DREnemy.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/DRAIController.h"

void UDREnemyAttributeSet::HandleIncomingDamage(const FEffectProperties& Props)
{
	float LocalIncomingDamage = GetIncomingDamage();
	SetIncomingDamage(0.f);
	if (LocalIncomingDamage > 0.f)
	{
		// 데미지가 발생하면 전투 상태 진입
		NotifyEnterCombat(Props);

		if (ADREnemy* Enemy = Cast<ADREnemy>(Props.TargetAvatarActor))
		{
			if(UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
			{
				if (ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Buff_Elite))
				{
					LocalIncomingDamage *= EliteBuffModifier;
					FMath::RoundToFloat(LocalIncomingDamage);
				}
			}
			if (ADRAIController* AIC = Cast<ADRAIController>(Enemy->GetController()))
			{
				AIC->UpdateCombatTime();
			}
		}

		const float NewHealth = GetHealth() - LocalIncomingDamage;
		SetHealth(FMath::Clamp(NewHealth, 0.f, GetMaxHealth()));

		if (ADREnemy* Enemy = Cast<ADREnemy>(Props.TargetAvatarActor))
		{
			if (ADRAIController* AIController = Cast<ADRAIController>(Enemy->GetController()))
			{
				UBlackboardComponent* BB = AIController->GetBlackboardComponent();

				// FirstAttacker가 없는 경우에만 설정
				if (!BB->GetValueAsBool("HasFirstAttacker"))
				{
					// 첫 공격자 설정
					BB->SetValueAsObject("FirstAttacker", Props.SourceAvatarActor);
					BB->SetValueAsBool("HasFirstAttacker", true);

					// 현재 타겟도 첫 공격자로 설정
					BB->SetValueAsObject("TargetToFollow", Props.SourceAvatarActor);
				}
				BB->SetValueAsObject("AttackingPlayer", Props.SourceAvatarActor);

				if (NewHealth <= GetMaxHealth() * 0.3f)
				{
					BB->SetValueAsBool("IsHealthLow", true);
					BB->SetValueAsBool("IsInitialized", false);
				}
			}

			const float HealthPercent = GetHealth() / GetMaxHealth();
			if (HealthPercent <= Enemy->EnrageHealthThreshold && !Enemy->bIsEnraged && Enemy->bIsPhase3Enemy)
			{
				Enemy->TriggerEnrage();
			}
		}

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

				// 넉백 상태 설정 추가
				if (ADREnemy* Enemy = Cast<ADREnemy>(Props.TargetCharacter))
				{
					Enemy->SetKnockbackState(true);

					// 넉백 종료 타이머 (안전장치)
					FTimerHandle KnockbackEndTimer;
					Props.TargetCharacter->GetWorld()->GetTimerManager().SetTimer(
						KnockbackEndTimer,
						[Enemy]()
						{
							if (IsValid(Enemy))
							{
								Enemy->SetKnockbackState(false);
							}
						},
						1.5f, // 최대 1.5초 후 자동 해제
						false
					);
				}
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
