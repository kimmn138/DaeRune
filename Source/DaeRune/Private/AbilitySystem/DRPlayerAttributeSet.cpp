// Copyright DaeRune


#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "Interaction/CombatInterface.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "DRGameplayTags.h"
#include "GameFramework/Character.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"

void UDRPlayerAttributeSet::EnterCorruptedState(const FEffectProperties& Props)
{
	if (bCorrupted) return;

	bCorrupted = true;

	// 체력 설정
	SetMaxHealth(CORRUPT_MAX_HEALTH);
	SetHealth(CORRUPT_MAX_HEALTH);

	// PlayerState에 알림 (태그 관리와 회복 중지를 PlayerState에서 처리)
	if (Props.TargetAvatarActor)
	{
		if (APlayerController* PC = Cast<APlayerController>(Props.TargetController))
		{
			if (ADRPlayerState* DRPS = PC->GetPlayerState<ADRPlayerState>())
			{
				DRPS->SetCorruptedState(true);
			}

			// 컨트롤러에 알림
			if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(PC))
			{
				DRPC->OnCorruptedStateChanged(true);
			}
		}
	}
}

void UDRPlayerAttributeSet::ExitCorruptedState(const FEffectProperties& Props)
{
	if (!bCorrupted) return;

	bCorrupted = false;

	// 체력 복원
	SetMaxHealth(NORMAL_MAX_HEALTH);
	SetHealth(CONTAINER_HEALTH);

	// PlayerState에 알림
	if (Props.TargetAvatarActor)
	{
		if (APlayerController* PC = Cast<APlayerController>(Props.TargetController))
		{
			if (ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>())
			{
				PS->SetCorruptedState(false);
			}

			// 컨트롤러에 알림
			if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(PC))
			{
				DRPC->OnCorruptedStateChanged(false);
			}
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
		int32 ContainerIndex = FMath::FloorToInt(NewHealth / CONTAINER_HEALTH);
		if (FMath::IsNearlyEqual(NewHealth, ContainerIndex * CONTAINER_HEALTH))
		{
			ContainerIndex = FMath::Max(0, ContainerIndex - 1);
		}
		ContainerIndex = FMath::Clamp(ContainerIndex, 0, NUM_CONTAINERS - 1);

		float HealthInContainer = NewHealth - (ContainerIndex * CONTAINER_HEALTH);

		if (RemainingDamage < HealthInContainer)
		{
			NewHealth -= RemainingDamage;
			break;
		}

		float OverflowDamage = RemainingDamage - HealthInContainer;

		// 오버플로우 체크
		if (OverflowDamage <= (Damage * OVERFLOW_THRESHOLD))
		{
			NewHealth = ContainerIndex * CONTAINER_HEALTH + 1.f;
			break;
		}

		NewHealth = ContainerIndex * CONTAINER_HEALTH;
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

	// 스킬로 인한 회복인지 체크 (SourceAvatarActor가 있으면 스킬/아이템 회복)
	const bool bIsActiveHealing = Props.SourceAvatarActor != nullptr &&
		Props.SourceAvatarActor != Props.TargetAvatarActor;

	if (bCorrupted && bIsActiveHealing)
	{
		// 아군 스킬로 인한 회복 시 부패 상태 해제
		ExitCorruptedState(Props);
	}
	else
	{
		// 정상 상태 회복
		const float NewHealth = GetHealth() + LocalIncomingHealing;
		SetHealth(FMath::Clamp(NewHealth, 0.f, GetMaxHealth()));
	}
}
