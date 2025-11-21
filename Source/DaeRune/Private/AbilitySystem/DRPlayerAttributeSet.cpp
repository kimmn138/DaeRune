// Copyright DaeRune


#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "Interaction/CombatInterface.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "DRGameplayTags.h"
#include "GameFramework/Character.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "Character/DRCharacter.h"

void UDRPlayerAttributeSet::SetContainerInfo(int32 InNumContainers, float InContainerHealth)
{
	NumContainers = InNumContainers;
	ContainerHealth = InContainerHealth;
}

int32 UDRPlayerAttributeSet::GetCurrentContainerIndex() const
{
	const float CurrentHealth = GetHealth();
	if (CurrentHealth <= 0.f) return -1;

	int32 ContainerIndex = FMath::FloorToInt(CurrentHealth / ContainerHealth);

	// ��Ȯ�� �����̳� ��迡 �ִ� ���
	if (FMath::IsNearlyEqual(CurrentHealth, ContainerIndex * ContainerHealth))
	{
		ContainerIndex = FMath::Max(0, ContainerIndex - 1);
	}

	return FMath::Clamp(ContainerIndex, 0, NumContainers - 1);
}

void UDRPlayerAttributeSet::EnterCorruptedState(const FEffectProperties& Props)
{
	if (bCorrupted) return;

	bCorrupted = true;

	// ���� ���·� ü�� ����
	SetMaxHealth(GetCorruptMaxHealth());
	SetHealth(GetCorruptMaxHealth());

	// PlayerState�� �˸�
	if (Props.TargetAvatarActor && Props.TargetController)
	{
		if (ADRPlayerState* DRPS = Props.TargetController->GetPlayerState<ADRPlayerState>())
		{
			DRPS->SetCorruptedState(true);
		}

		if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(Props.TargetController))
		{
			DRPC->CorruptedStateChanged(true);
		}
	}
}

void UDRPlayerAttributeSet::ExitCorruptedState(const FEffectProperties& Props)
{
	if (!bCorrupted) return;

	bCorrupted = false;

	// ���� ���·� ���� (�ִ� ü���� �������)
	const float NormalMaxHealth = NumContainers * ContainerHealth;
	SetMaxHealth(NormalMaxHealth);
	SetHealth(ContainerHealth); // ù ��° �����̳ʸ� ȸ��

	// PlayerState�� �˸�
	if (Props.TargetAvatarActor && Props.TargetController)
	{
		if (ADRPlayerState* PS = Props.TargetController->GetPlayerState<ADRPlayerState>())
		{
			PS->SetCorruptedState(false);
		}

		if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(Props.TargetController))
		{
			DRPC->CorruptedStateChanged(false);
		}
	}
}

void UDRPlayerAttributeSet::HandleIncomingDamage(const FEffectProperties& Props)
{
	float LocalIncomingDamage = GetIncomingDamage();
	SetIncomingDamage(0.f);

	if (LocalIncomingDamage <= 0.f) return;

	// ���� ���� ���� �˸�
	NotifyEnterCombat(Props);

	if (ADRPlayerState* PS = Cast<ADRPlayerState>(Props.TargetAvatarActor))
	{
		if(UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			if (ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Debuff_Elite))
			{
				LocalIncomingDamage *= EliteDebuffModifier;
				FMath::RoundToFloat(LocalIncomingDamage);
			}
		}
	}

	// ���� ���� ó��
	if (bCorrupted)
	{
		ProcessCorruptedDamage(Props, LocalIncomingDamage);
	}
	else
	{
		ProcessNormalDamage(Props, LocalIncomingDamage);
	}

	// ���� ó��
	ShowFloatingText(Props, LocalIncomingDamage);

	if (UDRAbilitySystemLibrary::IsSuccessfulDebuff(Props.EffectContextHandle))
	{
		Debuff(Props);
	}
}

void UDRPlayerAttributeSet::ProcessCorruptedDamage(const FEffectProperties& Props, float Damage)
{
	const float NewHealth = GetHealth() - Damage;
	SetHealth(FMath::Clamp(NewHealth, 0.f, GetMaxHealth()));

	if (NewHealth <= 0.f)
	{
		// ���� ���¿��� ��� ���
		if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Props.TargetAvatarActor))
		{
			CombatInterface->Die(UDRAbilitySystemLibrary::GetDeathImpulse(Props.EffectContextHandle));
		}
	}
	else
	{
		ApplyHitReactAndKnockback(Props);
	}
}

void UDRPlayerAttributeSet::ProcessNormalDamage(const FEffectProperties& Props, float Damage)
{
	float NewHealth = CalculateContainerDamage(GetHealth(), Damage);
	SetHealth(FMath::Clamp(NewHealth, 0.f, GetMaxHealth()));

	if (NewHealth <= 0.f)
	{
		// ���� ���·� ��ȯ
		EnterCorruptedState(Props);
	}
	else
	{
		ApplyHitReactAndKnockback(Props);
	}
}

float UDRPlayerAttributeSet::CalculateContainerDamage(float CurrentHealth, float Damage) const
{
	float NewHealth = CurrentHealth;
	float RemainingDamage = Damage;

	while (RemainingDamage > 0.f && NewHealth > 0.f)
	{
		int32 ContainerIndex = FMath::FloorToInt(NewHealth / ContainerHealth);
		if (FMath::IsNearlyEqual(NewHealth, ContainerIndex * ContainerHealth))
		{
			ContainerIndex = FMath::Max(0, ContainerIndex - 1);
		}
		ContainerIndex = FMath::Clamp(ContainerIndex, 0, NumContainers - 1);

		float HealthInContainer = NewHealth - (ContainerIndex * ContainerHealth);

		if (RemainingDamage < HealthInContainer)
		{
			NewHealth -= RemainingDamage;
			break;
		}

		float OverflowDamage = RemainingDamage - HealthInContainer;

		// �����÷ο� üũ
		if (OverflowDamage <= (Damage * OVERFLOW_THRESHOLD))
		{
			NewHealth = ContainerIndex * ContainerHealth + 1.f;
			break;
		}

		NewHealth = ContainerIndex * ContainerHealth;
		RemainingDamage = OverflowDamage;
	}

	return NewHealth;
}

void UDRPlayerAttributeSet::ApplyHitReactAndKnockback(const FEffectProperties& Props)
{
	// Hit React
	if (Props.TargetCharacter && Props.TargetCharacter->Implements<UCombatInterface>() &&
		!ICombatInterface::Execute_IsBeingShocked(Props.TargetCharacter))
	{
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(FDRGameplayTags::Get().Effects_HitReact);
		Props.TargetASC->TryActivateAbilitiesByTag(TagContainer);
	}

	// Knockback
	const FVector& KnockbackForce = UDRAbilitySystemLibrary::GetKnockbackForce(Props.EffectContextHandle);
	if (!KnockbackForce.IsNearlyZero(1.f) && Props.TargetCharacter)
	{
		Props.TargetCharacter->LaunchCharacter(KnockbackForce, true, true);
	}
}

void UDRPlayerAttributeSet::HandleIncomingHealing(const FEffectProperties& Props)
{
	const float LocalIncomingHealing = GetIncomingHealing();
	SetIncomingHealing(0.f);

	if (LocalIncomingHealing <= 0.f) return;

	const float CurrentHealth = GetHealth();
	const float MaxHealthValue = GetMaxHealth();

	// ���� ���� Ȯ��
	if (IsCorrupted())
	{
		// ���� ��ȭ ó��
		HandleCorruptionPurification(Props, LocalIncomingHealing);
		return;
	}

	// ���� ����: �׳� ���� ���� (�����̳� �����ϰ�)
	float NewHealth = FMath::Min(CurrentHealth + LocalIncomingHealing, MaxHealthValue);
	SetHealth(NewHealth);
}

void UDRPlayerAttributeSet::HandleCorruptionPurification(const FEffectProperties& Props, float HealAmount)
{
	UE_LOG(LogTemp, Log, TEXT("Purifying corruption with healing"));

	bCorrupted = false;

	// 1. ���� ���·� ���� (�ִ� ü���� �������)
	const float NormalMaxHealth = NumContainers * ContainerHealth;
	SetMaxHealth(NormalMaxHealth);
	SetHealth(ContainerHealth);

	// DRPlayerState�� SetCorruptedState ���
	if (ADRCharacter* Owner = Cast<ADRCharacter>(Props.TargetAvatarActor))
	{
		if (ADRPlayerState* PlayerState = Owner->GetPlayerState<ADRPlayerState>())
		{
			// false�� �����Ͽ� ���� ���� ����
			PlayerState->SetCorruptedState(false);

			UE_LOG(LogTemp, Log, TEXT("Corruption purified using SetCorruptedState"));
		}
	}
}