// Copyright DaeRune


#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "Interaction/CombatInterface.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "DRGameplayTags.h"
#include "GameFramework/Character.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"

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

	// 정확히 컨테이너 경계에 있는 경우
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

	// 부패 상태로 체력 설정
	SetMaxHealth(GetCorruptMaxHealth());
	SetHealth(GetCorruptMaxHealth());

	// PlayerState에 알림
	if (Props.TargetAvatarActor && Props.TargetController)
	{
		if (ADRPlayerState* DRPS = Props.TargetController->GetPlayerState<ADRPlayerState>())
		{
			DRPS->SetCorruptedState(true);
		}

		if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(Props.TargetController))
		{
			DRPC->OnCorruptedStateChanged(true);
		}
	}
}

void UDRPlayerAttributeSet::ExitCorruptedState(const FEffectProperties& Props)
{
	if (!bCorrupted) return;

	bCorrupted = false;

	// 정상 상태로 복원 (최대 체력을 원래대로)
	const float NormalMaxHealth = NumContainers * ContainerHealth;
	SetMaxHealth(NormalMaxHealth);
	SetHealth(ContainerHealth); // 첫 번째 컨테이너만 회복

	// PlayerState에 알림
	if (Props.TargetAvatarActor && Props.TargetController)
	{
		if (ADRPlayerState* PS = Props.TargetController->GetPlayerState<ADRPlayerState>())
		{
			PS->SetCorruptedState(false);
		}

		if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(Props.TargetController))
		{
			DRPC->OnCorruptedStateChanged(false);
		}
	}
}

void UDRPlayerAttributeSet::HandleIncomingDamage(const FEffectProperties& Props)
{
	const float LocalIncomingDamage = GetIncomingDamage();
	SetIncomingDamage(0.f);

	if (LocalIncomingDamage <= 0.f) return;

	// 전투 상태 진입 알림
	NotifyEnterCombat(Props);

	// 부패 상태 처리
	if (bCorrupted)
	{
		ProcessCorruptedDamage(Props, LocalIncomingDamage);
	}
	else
	{
		ProcessNormalDamage(Props, LocalIncomingDamage);
	}

	// 공통 처리
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
		// 부패 상태에서 즉시 사망
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
		// 부패 상태로 전환
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

		// 오버플로우 체크
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

	// 스킬 회복 여부 판단
	const bool bIsActiveHealing = Props.SourceAvatarActor &&
		Props.SourceAvatarActor != Props.TargetAvatarActor;

	if (bCorrupted)
	{
		if (bIsActiveHealing)
		{
			ExitCorruptedState(Props);
		}
	}
	else
	{
		const float NewHealth = GetHealth() + LocalIncomingHealing;
		SetHealth(FMath::Min(NewHealth, GetMaxHealth()));
	}
}
