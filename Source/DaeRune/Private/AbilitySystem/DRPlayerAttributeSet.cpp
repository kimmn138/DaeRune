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
#include "Character/DRRobotVacuumCharacter.h"
#include "Game/DRLobbyGameMode.h"
#include "Game/DRProgressionConfig.h"
#include "Game/DRTutorialGameMode.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"

void UDRPlayerAttributeSet::SetContainerInfo(int32 InNumContainers, float InContainerHealth)
{
	NumContainers = FMath::Max(1, InNumContainers);

	// 칸 크기 = 최대 체력 / 칸 수 (UI 와 같은 규칙). 오염 중이거나 최대 체력이 아직 없을 때만 넘겨받은 값을 쓴다.
	const float MaxHealthValue = GetMaxHealth();
	ContainerHealth = (!bCorrupted && MaxHealthValue > 0.f)
		? MaxHealthValue / static_cast<float>(NumContainers)
		: InContainerHealth;
}

float UDRPlayerAttributeSet::GetBaseMaxHealthWithoutModifiers() const
{
	const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC) return NumContainers * ContainerHealth;

	// 모디파이어(칩 GE 등) 몫 = 현재값 − BaseValue. 오염 중에는 현재값이 100 이라 오염 직전 최대 체력(칸 수 × 칸 크기)에서 뺀다.
	const float ModifierOffset = GetMaxHealth() - ASC->GetNumericAttributeBase(GetMaxHealthAttribute());
	const float NormalMaxHealth = bCorrupted ? NumContainers * ContainerHealth : GetMaxHealth();
	return NormalMaxHealth - ModifierOffset;
}

void UDRPlayerAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// 칸 크기는 최대 체력을 따라간다 — 클래스 기본값·칩 GE 가 무엇이든 칸 수 × 칸 크기 = 최대 체력이 유지된다.
	// 오염 중(최대 체력 100)에는 갱신하지 않는다 — 오염 해제 때 칸 수 × 칸 크기로 원래 최대 체력을 되돌린다.
	if (Attribute == GetMaxHealthAttribute() && !bCorrupted && NewValue > 0.f && NumContainers > 0)
	{
		ContainerHealth = NewValue / static_cast<float>(NumContainers);
	}

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
	// ★Plan5(탑승칩)★ 칩과 무관하게 오염 최대 체력은 항상 100 — 현재값 기준으로 맞춘다 (D-87 · T-87)
	SetMaxHealthCurrentValue(GetCorruptMaxHealth());
	SetHealth(GetCorruptMaxHealth());

	// PlayerState에 알림 (플레이어 ASC 의 오너가 PlayerState 다)
	if (ADRPlayerState* DRPS = Cast<ADRPlayerState>(GetOwningActor()))
	{
		DRPS->SetCorruptedState(true);
	}
}

void UDRPlayerAttributeSet::ExitCorruptedState()
{
	if (!bCorrupted) return;

	bCorrupted = false;

	// 정상 상태로 복원 (최대 체력을 원래대로)
	const float NormalMaxHealth = NumContainers * ContainerHealth;
	SetMaxHealthCurrentValue(NormalMaxHealth);   // ★Plan5(탑승칩)★ (D-87 · T-87)
	SetHealth(ContainerHealth); // 첫 번째 컨테이너만 회복

	if (ADRPlayerState* DRPS = Cast<ADRPlayerState>(GetOwningActor()))
	{
		DRPS->SetCorruptedState(false);
	}
}

void UDRPlayerAttributeSet::HandleIncomingDamage(const FEffectProperties& Props)
{
	float LocalIncomingDamage = GetIncomingDamage();
	SetIncomingDamage(0.f);

	if (LocalIncomingDamage <= 0.f) return;

	ADRCharacter* TargetDRCharacter = Cast<ADRCharacter>(Props.TargetAvatarActor);

	// ★Plan5(탑승칩)★ ② 아래에 칩② 청소기가 있으면 이 피해는 그 청소기가 줄여서 대신 받는다 (D-83 · D-84).
	// ★맨 위에서 먼저 구한다★ — 바로 아래 피격 큐를 건너뛰어야 한다 (T-76)
	ADRRobotVacuumCharacter* RideGuard = UDRAbilitySystemLibrary::FindRideGuard(TargetDRCharacter);

	if (Props.TargetASC && !RideGuard)
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Props.TargetAvatarActor ? Props.TargetAvatarActor->GetActorLocation() : FVector::ZeroVector;
		CueParams.RawMagnitude = LocalIncomingDamage;

		Props.TargetASC->ExecuteGameplayCue(
			FDRGameplayTags::Get().GameplayCue_Player_Damage,
			CueParams
		);
	}

	// 전투 상태 진입 알림
	NotifyEnterCombat(Props);

	// 디버프/데미지 원인 적의 전투 상태 갱신
	if (ADREnemy* SourceEnemy = Cast<ADREnemy>(Props.SourceAvatarActor))
	{
		if (ADRAIController* AIC = Cast<ADRAIController>(SourceEnemy->GetController()))
		{
			AIC->UpdateCombatTime();
		}
	}

	// 부품을 들고 있으면 데미지 1.5배 + 들고 있던 부품을 떨어뜨린다
	// (★Plan5(탑승칩)★ 바깥 TargetDRCharacter 를 쓴다 — 같은 이름을 다시 선언하면 그림자 변수 오류 (T-91))
	if (TargetDRCharacter && TargetDRCharacter->IsCarryingPart())
	{
		LocalIncomingDamage *= CarryingPartDamageModifier;
		TargetDRCharacter->ForceDropCarriedPart();
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

	// ↑ 여기까지가 이 사람이 "최종적으로 받아야 할 피해" L 이다 (받는 쪽 배율 · 데미지 디버프 반영)

	// ★Plan5(탑승칩)★ ② 보호받는 사람 — L 의 20% 를 칩② 청소기로 넘기고, 피격 자체의 1칸 공유는 L 그대로 보낸다 (T-93).
	// 이 사람은 맞지 않았다. ★로비 클램프 · 오염 분기 전★ (T-75). 못 넘기면 그대로 받는다 (T-89)
	if (RideGuard && UDRAbilitySystemLibrary::TransferRideDamageToGuard(TargetDRCharacter, RideGuard, LocalIncomingDamage))
	{
		// ★디버프는 그대로 건다★ — 화상 DoT 같은 디버프 피해는 매 틱 이 관문을 다시 지나 20% 로 넘어간다 (D-84 · T-94)
		if (UDRAbilitySystemLibrary::IsSuccessfulDebuff(Props.EffectContextHandle))
		{
			Debuff(Props);
		}
		// 공유로 들어온 피해면 PropagateSharedDamage 안에서 멈춘다(재전파 방지)
		TargetDRCharacter->PropagateSharedDamage(LocalIncomingDamage, Props.EffectContextHandle);
		return;
	}

	// 로비/튜토리얼에서는 체력이 1 미만으로 내려가지 않도록 데미지 클램프
	if (ShouldPreventDeath())
	{
		const float MaxAllowedDamage = FMath::Max(0.f, GetHealth() - 1.f);
		LocalIncomingDamage = FMath::Min(LocalIncomingDamage, MaxAllowedDamage);
	}

	const bool bWasCorrupted = bCorrupted;   // ★Plan5(탑승칩)★ ③ 이번 피격의 오염 진입 감지 (T-79)

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

	// 탑승 데미지 공유 (Plan3 §5.4): 마운트 링크 위/아래 1홉으로 전파.
	// MountShared 태그가 붙은 데미지는 PropagateSharedDamage 내부에서 재전파가 차단된다.
	if (TargetDRCharacter)
	{
		TargetDRCharacter->PropagateSharedDamage(LocalIncomingDamage, Props.EffectContextHandle);
	}

	// ★Plan5(탑승칩)★ ③ 비상 탈출 — ★반드시 맨 끝★. 하차가 공유보다 먼저면 이번 공유분이 탑에 다 안 나간다 (T-78)
	UDRAbilitySystemLibrary::TryRideEmergencyEject(TargetDRCharacter, !bWasCorrupted && bCorrupted);
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
	if (Props.TargetASC && Props.TargetCharacter && Props.TargetCharacter->Implements<UCombatInterface>() &&
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

	// 오염 중 회복 = 정화 (힐량과 무관하게 오염 해제 + 첫 칸)
	if (IsCorrupted())
	{
		ExitCorruptedState();
		return;
	}

	// 정상 상태: 그냥 힐량 적용 (컨테이너 무관하게)
	float NewHealth = FMath::Min(CurrentHealth + LocalIncomingHealing, MaxHealthValue);
	SetHealth(NewHealth);
}

void UDRPlayerAttributeSet::SetMaxHealthCurrentValue(float Desired)
{
	// ★Plan5(탑승칩)★ SetMaxHealth 는 BaseValue 를 바꾼다 — GE_Upgrade_Stats(Infinite, AddBase)의 칩 가산이 그 위에 다시 붙으므로
	// 그 몫(현재 − Base)을 빼고 넣는다. 칩이 없으면 몫이 0 이라 예전과 같다.
	// ★MaxHealth 에 곱 모디파이어가 없다는 전제★ — Data.Upgrade.MaxHealth.Mult 는 항상 1.0 (Plan5 D-87 · T-88)
	const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	const float ModifierOffset = ASC ? GetMaxHealth() - ASC->GetNumericAttributeBase(GetMaxHealthAttribute()) : 0.f;
	SetMaxHealth(Desired - ModifierOffset);
}

void UDRPlayerAttributeSet::ReapplyCorruptMaxHealth(float KeepHealth)
{
	if (!bCorrupted) return;
	SetMaxHealthCurrentValue(GetCorruptMaxHealth());
	SetHealth(FMath::Clamp(KeepHealth, 0.f, GetMaxHealth()));
}

bool UDRPlayerAttributeSet::ShouldPreventDeath() const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	const AGameModeBase* GM = World->GetAuthGameMode();
	if (!GM) return false;

	return GM->IsA<ADRLobbyGameMode>() || GM->IsA<ADRTutorialGameMode>();
}
