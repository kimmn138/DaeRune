// Copyright DaeRune


#include "AbilitySystem/DREnemyAttributeSet.h"
#include "Interaction/CombatInterface.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "DRGameplayTags.h"
#include "GameFramework/Character.h"
#include "Character/DREnemy.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/DRAIController.h"
#include "Tutorial/DRTutorialManager.h"
#include "DRAbilityTypes.h"

void UDREnemyAttributeSet::HandleIncomingDamage(const FEffectProperties& Props)
{
	float LocalIncomingDamage = GetIncomingDamage();
	SetIncomingDamage(0.f);
	if (LocalIncomingDamage > 0.f)
	{
		if (Props.TargetASC)
		{
			FGameplayCueParameters CueParams;
			CueParams.Location = Props.TargetAvatarActor ? Props.TargetAvatarActor->GetActorLocation() : FVector::ZeroVector;
			CueParams.RawMagnitude = LocalIncomingDamage;

			Props.TargetASC->ExecuteGameplayCue(
				FDRGameplayTags::Get().GameplayCue_Enemy_Damage,
				CueParams
			);
		}

		// �������� �߻��ϸ� ���� ���� ����
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

		// 튜토리얼 더미: 체력을 최소 1로 유지 (무적) + 적중 보고
		if (ADREnemy* DummyCheck = Cast<ADREnemy>(Props.TargetAvatarActor))
		{
			if (DummyCheck->bIsTutorialDummy)
			{
				SetHealth(FMath::Max(1.f, NewHealth));

				// 튜토리얼 매니저에 적중 보고 (AbilityTag로 구분)
				if (DummyCheck->TutorialManagerRef.IsValid())
				{
					if (ADRTutorialManager* TM = Cast<ADRTutorialManager>(DummyCheck->TutorialManagerRef.Get()))
					{
						FGameplayTagContainer AbilityTags;
						if (const FDRGameplayEffectContext* DRContext = static_cast<const FDRGameplayEffectContext*>(Props.EffectContextHandle.Get()))
						{
							AbilityTags = DRContext->GetSourceAbilityTags();
						}
						TM->ReportDamageHit(AbilityTags);
					}
				}

				// 히트 리액션은 정상 재생
				if (Props.TargetCharacter->Implements<UCombatInterface>() && !ICombatInterface::Execute_IsBeingShocked(Props.TargetCharacter))
				{
					FGameplayTagContainer TagContainer;
					TagContainer.AddTag(FDRGameplayTags::Get().Effects_HitReact);
					Props.TargetASC->TryActivateAbilitiesByTag(TagContainer);
				}

				ShowFloatingText(Props, LocalIncomingDamage);
				return;
			}
		}

		SetHealth(FMath::Clamp(NewHealth, 0.f, GetMaxHealth()));

		if (ADREnemy* Enemy = Cast<ADREnemy>(Props.TargetAvatarActor))
		{
			if (ADRAIController* AIController = Cast<ADRAIController>(Enemy->GetController()))
			{
				UBlackboardComponent* BB = AIController->GetBlackboardComponent();

				// 공격자가 플레이어인 경우에만 타겟 설정 (적끼리 공격 시 타겟팅 방지)
				const bool bSourceIsPlayer = Props.SourceAvatarActor && Props.SourceAvatarActor->ActorHasTag(FName("Player"));

				// FirstAttacker가 없고, 부품을 들고 도망치는 적이 아닌 경우에만 설정
				if (bSourceIsPlayer && !BB->GetValueAsBool("HasFirstAttacker") && !Enemy->bCarriesPart)
				{
					// 첫 공격자 설정
					BB->SetValueAsObject("FirstAttacker", Props.SourceAvatarActor);
					BB->SetValueAsBool("HasFirstAttacker", true);

					// 추적 타겟도 첫 공격자로 설정
					BB->SetValueAsObject("TargetToFollow", Props.SourceAvatarActor);

					// 첫 공격자를 향해 SetFocus
					AIController->SetFocus(Props.SourceAvatarActor);

					// 어그로 상태 설정 (AnimBP용)
					Enemy->bIsAggroed = true;

					// GAS 상태 태그 추가
					if (UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent())
					{
						EnemyASC->AddLooseGameplayTag(FDRGameplayTags::Get().State_Aggroed);
					}
				}
				if (bSourceIsPlayer)
				{
					BB->SetValueAsObject("AttackingPlayer", Props.SourceAvatarActor);
				}

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

				// �˹� ���� ���� �߰�
				if (ADREnemy* Enemy = Cast<ADREnemy>(Props.TargetCharacter))
				{
					Enemy->SetKnockbackState(true);

					// �˹� ���� Ÿ�̸� (������ġ)
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
						1.5f, // �ִ� 1.5�� �� �ڵ� ����
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
