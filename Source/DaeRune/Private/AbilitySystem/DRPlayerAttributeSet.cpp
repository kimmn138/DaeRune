// Copyright DaeRune


#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "Interaction/CombatInterface.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "DRGameplayTags.h"
#include "AI/DRAIController.h"
#include "GameFramework/Character.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "Character/DRCharacter.h"
#include "Character/DREnemy.h"
#include "Game/DRLobbyGameMode.h"
#include "Game/DRTutorialGameMode.h"
#include "Engine/World.h"

void UDRPlayerAttributeSet::SetContainerInfo(int32 InNumContainers, float InContainerHealth)
{
	NumContainers = InNumContainers;
	ContainerHealth = InContainerHealth;
}

void UDRPlayerAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetHealthAttribute() && !bCorrupted)
	{
		// 체력값으로부터 0-based 컨테이너 인덱스 계산 (인덱스 0 = 마지막 컨테이너)
		auto ContainerIndexFromHealth = [this](float HealthValue) -> int32
		{
			if (HealthValue <= 0.f) return -1;
			int32 Idx = FMath::FloorToInt(HealthValue / ContainerHealth);
			if (FMath::IsNearlyEqual(HealthValue, Idx * ContainerHealth))
			{
				Idx = FMath::Max(0, Idx - 1);
			}
			return FMath::Clamp(Idx, 0, NumContainers - 1);
		};

		const int32 OldIndex = ContainerIndexFromHealth(OldValue);
		const int32 NewIndex = ContainerIndexFromHealth(NewValue);

		// 컨테이너 N → 1 전이 시점에만 큐 발동 (1개 상태에서 매 피격마다 울리지 않도록)
		if (OldIndex > 0 && NewIndex == 0)
		{
			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				FGameplayCueParameters CueParams;
				if (AActor* Avatar = ASC->GetAvatarActor())
				{
					CueParams.Location = Avatar->GetActorLocation();
				}

				ASC->ExecuteGameplayCue(
					FDRGameplayTags::Get().GameplayCue_Player_LowHealth,
					CueParams
				);
			}
		}
	}
}

int32 UDRPlayerAttributeSet::GetCurrentContainerIndex() const
{
	const float CurrentHealth = GetHealth();
	if (CurrentHealth <= 0.f) return -1;

	int32 ContainerIndex = FMath::FloorToInt(CurrentHealth / ContainerHealth);

	// 占쏙옙확占쏙옙 占쏙옙占쏙옙占싱놂옙 占쏙옙瓦?占쌍댐옙 占쏙옙占?
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

	// 占쏙옙占쏙옙 占쏙옙占승뤄옙 체占쏙옙 占쏙옙占쏙옙
	SetMaxHealth(GetCorruptMaxHealth());
	SetHealth(GetCorruptMaxHealth());

	// PlayerState占쏙옙 占싯몌옙
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

	// 占쏙옙占쏙옙 占쏙옙占승뤄옙 占쏙옙占쏙옙 (占쌍댐옙 체占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占?
	const float NormalMaxHealth = NumContainers * ContainerHealth;
	SetMaxHealth(NormalMaxHealth);
	SetHealth(ContainerHealth); // 첫 占쏙옙째 占쏙옙占쏙옙占싱너몌옙 회占쏙옙

	// PlayerState占쏙옙 占싯몌옙
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

	if (Props.TargetASC)
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Props.TargetAvatarActor ? Props.TargetAvatarActor->GetActorLocation() : FVector::ZeroVector;
		CueParams.RawMagnitude = LocalIncomingDamage;

		Props.TargetASC->ExecuteGameplayCue(
			FDRGameplayTags::Get().GameplayCue_Player_Damage,
			CueParams
		);
	}

	// 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占싯몌옙
	NotifyEnterCombat(Props);

	// ?붾쾭???곕?吏 ?먯씤 ?곸쓽 ?꾪닾 ?곹깭 媛깆떊
	if (ADREnemy* SourceEnemy = Cast<ADREnemy>(Props.SourceAvatarActor))
	{
		if (ADRAIController* AIC = Cast<ADRAIController>(SourceEnemy->GetController()))
		{
			AIC->UpdateCombatTime();
		}
	}

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

	// 로비/튜토리얼에서는 체력이 1 미만으로 내려가지 않도록 데미지 클램프
	if (ShouldPreventDeath())
	{
		const float MaxAllowedDamage = FMath::Max(0.f, GetHealth() - 1.f);
		LocalIncomingDamage = FMath::Min(LocalIncomingDamage, MaxAllowedDamage);
	}

	// 占쏙옙占쏙옙 占쏙옙占쏙옙 처占쏙옙
	if (bCorrupted)
	{
		ProcessCorruptedDamage(Props, LocalIncomingDamage);
	}
	else
	{
		ProcessNormalDamage(Props, LocalIncomingDamage);
	}

	// 占쏙옙占쏙옙 처占쏙옙
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
		// 占쏙옙占쏙옙 占쏙옙占승울옙占쏙옙 占쏙옙占?占쏙옙占?
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
		// 占쏙옙占쏙옙 占쏙옙占승뤄옙 占쏙옙환
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

		// 占쏙옙占쏙옙占시로울옙 체크
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
		// 피격 표정 트리거 (서버 → 모든 클라이언트). 태그 이벤트는 카운트 누적으로
		// 1회만 발화하므로, 표정은 명시적 멀티캐스트로 매 피격마다 발화시킨다.
		if (ADRCharacter* TargetDRChar = Cast<ADRCharacter>(Props.TargetCharacter))
		{
			TargetDRChar->MulticastPlayHitReactFacial();
		}

FGameplayTagContainer TagContainer;
		TagContainer.AddTag(FDRGameplayTags::Get().Effects_HitReact);
		const bool bSuccess = Props.TargetASC->TryActivateAbilitiesByTag(TagContainer);
if (!bSuccess && Props.TargetASC)
		{
			FGameplayTagContainer ActivatableAbilities;
			TArray<FGameplayAbilitySpec*> MatchingSpecs;
			Props.TargetASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(TagContainer, MatchingSpecs);
for (const FGameplayAbilitySpec* Spec : MatchingSpecs)
			{
}
		}
	}
	else
	{
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

	// 占쏙옙占쏙옙 占쏙옙占쏙옙 확占쏙옙
	if (IsCorrupted())
	{
		// 占쏙옙占쏙옙 占쏙옙화 처占쏙옙
		HandleCorruptionPurification(Props, LocalIncomingHealing);
		return;
	}

	// 占쏙옙占쏙옙 占쏙옙占쏙옙: 占쌓놂옙 占쏙옙占쏙옙 占쏙옙占쏙옙 (占쏙옙占쏙옙占싱놂옙 占쏙옙占쏙옙占싹곤옙)
	float NewHealth = FMath::Min(CurrentHealth + LocalIncomingHealing, MaxHealthValue);
	SetHealth(NewHealth);
}

bool UDRPlayerAttributeSet::ShouldPreventDeath() const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	const AGameModeBase* GM = World->GetAuthGameMode();
	if (!GM) return false;

	return GM->IsA<ADRLobbyGameMode>() || GM->IsA<ADRTutorialGameMode>();
}

void UDRPlayerAttributeSet::HandleCorruptionPurification(const FEffectProperties& Props, float HealAmount)
{
bCorrupted = false;

	// 1. 占쏙옙占쏙옙 占쏙옙占승뤄옙 占쏙옙占쏙옙 (占쌍댐옙 체占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占?
	const float NormalMaxHealth = NumContainers * ContainerHealth;
	SetMaxHealth(NormalMaxHealth);
	SetHealth(ContainerHealth);

	// DRPlayerState占쏙옙 SetCorruptedState 占쏙옙占?
	if (ADRCharacter* Owner = Cast<ADRCharacter>(Props.TargetAvatarActor))
	{
		if (ADRPlayerState* PlayerState = Owner->GetPlayerState<ADRPlayerState>())
		{
			// false占쏙옙 占쏙옙占쏙옙占싹울옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙
			PlayerState->SetCorruptedState(false);
}
	}
}
