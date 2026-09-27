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
#include "Game/DRProgressionConfig.h"
#include "Game/DRTutorialGameMode.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"

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

	// 부품을 들고 있으면 데미지 1.5배 + 들고 있던 부품을 떨어뜨린다
	if (ADRCharacter* TargetDRCharacter = Cast<ADRCharacter>(Props.TargetCharacter))
	{
		if (TargetDRCharacter->IsCarryingPart())
		{
			LocalIncomingDamage *= CarryingPartDamageModifier;
			TargetDRCharacter->ForceDropCarriedPart();
		}
	}

	// 업그레이드 칩의 "받는 피해" 배율 (돌파 칩의 단점/방어 효과). 0 하한 — 회복으로 뒤집히지 않게 한다.
	if (Props.TargetController)
	{
		if (const ADRPlayerState* TargetPS = Props.TargetController->GetPlayerState<ADRPlayerState>())
		{
			LocalIncomingDamage = FMath::Max(0.f,
				TargetPS->GetUpgradeRuntime().Apply(EDRUpgradeStat::DamageTaken, LocalIncomingDamage));
		}
	}

	// ★Plan3(공용칩) §17.7.4★ 시너지 오라: 받는 피해 감소.
	// 스택 수는 ASC 태그 카운트 = 나를 버프하고 있는 팀원 수다.
	if (Props.TargetASC)
	{
		const int32 SynergyStacks =
			Props.TargetASC->GetGameplayTagCount(FDRGameplayTags::Get().Buff_Upgrade_Synergy);
		if (SynergyStacks > 0)
		{
			const FDRChipTuning& Tuning =
				UDRAbilitySystemLibrary::GetChipTuning(Props.TargetAvatarActor);

			// 스택이 아무리 쌓여도 무적이 되지 않게 하한을 둔다
			const float Ratio = FMath::Max(
				Tuning.SynergyMinDamageTakenRatio,
				1.f - Tuning.SynergyDamageTakenPerStack * SynergyStacks);

			LocalIncomingDamage *= Ratio;
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

	// 탑승 데미지 공유 (Plan3 §5.4): 마운트 링크 위/아래 1홉으로 전파.
	// MountShared 태그가 붙은 데미지는 PropagateSharedDamage 내부에서 재전파가 차단된다.
	if (ADRCharacter* MountLinkedCharacter = Cast<ADRCharacter>(Props.TargetAvatarActor))
	{
		MountLinkedCharacter->PropagateSharedDamage(LocalIncomingDamage, Props.EffectContextHandle);
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
	float NewHealth;

	// ★Plan3(공용칩) §17.6.2★ 광폭화 칩: 피해량과 무관하게 현재 컨테이너를 통째로 비운다.
	//
	// HandleIncomingDamage 본체가 아니라 여기에 두는 이유:
	//  ① 오염 상태(ProcessCorruptedDamage)에는 컨테이너 개념이 없다
	//  ② 모든 배율(부품 1.5배 / DamageTaken / 시너지)이 끝난 뒤라 "피해량 무관"의 의미가 분명하다
	if (UDRAbilitySystemLibrary::HasUpgradeFeature(
			Props.TargetASC, FDRGameplayTags::Get().Upgrade_Feature_Combat_Berserk))
	{
		// GetCurrentContainerIndex() 는 칸 경계에 정확히 서 있을 때 한 칸 아래로 보정해 주므로,
		// 경계에서도 "다음 칸"이 아니라 "현재 칸"을 비우게 된다.
		const int32 ContainerIndex = FMath::Max(0, GetCurrentContainerIndex());
		NewHealth = ContainerIndex * ContainerHealth;

		// 로비/튜토리얼에서는 죽지 않아야 한다 — 칸은 비우되 체력 1 을 남긴다
		if (NewHealth <= 0.f && ShouldPreventDeath())
		{
			NewHealth = 1.f;
		}
	}
	else
	{
		NewHealth = CalculateContainerDamage(GetHealth(), Damage);
	}

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

	// ★Plan3(공용칩) §17.6.3★ 회복 차단 칩: 들어온 힐을 그대로 버린다.
	// ★IncomingHealing 을 0 으로 비운 "뒤"에 반환해야★ 다음 힐이 누적되지 않는다.
	// 오염 정화도 이 경로를 타므로 함께 막힌다(열린 결정 O-9 — 정화만 허용하려면
	// 아래 조건에 !IsCorrupted() 를 추가한다).
	if (UDRAbilitySystemLibrary::HasUpgradeFeature(
			Props.TargetASC, FDRGameplayTags::Get().Upgrade_Feature_Combat_NoHealing))
	{
		return;
	}

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
