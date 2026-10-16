// Copyright DaeRune


#include "AbilitySystem/DREnemyAttributeSet.h"
#include "Interaction/CombatInterface.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
// FDRChipTuning — 받는 피해 증가 배율을 읽는다 (Plan5(물대포칩))
#include "Game/DRProgressionConfig.h"
#include "DRGameplayTags.h"
#include "GameFramework/Character.h"
#include "Character/DREnemy.h"
#include "Player/DRPlayerState.h"
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

		// ★Plan5(물대포칩)★ 받는 피해 증가 표식(Debuff.Vulnerable).
		//
		// ★ExecCalc_Damage 가 아니라 여기인 이유★: 출혈/화상 같은 디버프 DoT 은
		// IncomingDamage 에 SetByCaller 로 ★직접★ 꽂혀 ExecCalc 를 지나지 않는다.
		// "모든 피해"를 늘리려면 IncomingDamage 가 반드시 지나는 이 지점이어야 한다. (D-10)
		//
		// ★중첩하지 않는다★ — 태그 ★카운트★ 가 아니라 보유 여부만 본다.
		// 카운트로 곱하면 정원로봇 N명이 같은 적을 쏠 때 배율이 N제곱으로 튄다. (D-11)
		if (Props.TargetASC && Props.TargetASC->HasMatchingGameplayTag(
				FDRGameplayTags::Get().Debuff_Vulnerable))
		{
			const FDRChipTuning& ChipTuning =
				UDRAbilitySystemLibrary::GetChipTuning(Props.TargetAvatarActor);
			LocalIncomingDamage *= (1.f + FMath::Max(0.f, ChipTuning.WaterPump.VulnerableDamageTakenBonus));
		}

		// ★Plan5(공기탄칩)★ ② 흡착 — 스택당 받는 피해 +5%(최대 3). 취약과 같은 자리 · 같은 이유(D-10) — 모든 피해에 (D-98).
		// ★이번 적중이 쌓을 스택은 아직 없다★ — 직전 스택으로 계산하고, 스택은 아래에서 쌓는다 (D-97)
		LocalIncomingDamage *= UDRAbilitySystemLibrary::GetAirShotDragDamageTakenMultiplier(Props.TargetASC);

		// ★Plan5(도발칩)★ ① 약점 노출 — 도발된 적이 받는 모든 피해 ×1.15(그 도발을 건 벨보이가 칩①일 때만).
		// 취약 · 흡착과 같은 자리 · 같은 이유 — DoT 까지 "모든 피해"다 (D-10 · D-129). 아래 흡혈 · 숫자 표시가 이 값을 본다
		LocalIncomingDamage *= UDRAbilitySystemLibrary::GetTauntDamageTakenMultiplier(Props.TargetASC);

		// ★Plan5(발톱칩)★ 흡혈 — "적에게 준 피해만큼" 시전자 체력 회복.
		//
		// ★여기서 지급하는 이유★: 엘리트 ×0.7 / 취약 ×1.5 / Phase1 ×0.7 이 전부 끝난
		//   ★최종 피해★ 를 알 수 있는 유일한 지점이다. 어빌리티 쪽에서 지급하면
		//   화면에 뜬 숫자와 회복량이 어긋난다.
		//
		// ★반드시 SetHealth 보다 먼저★ — GetHealth() 로 남은 체력을 읽어
		//   ★오버킬분을 잘라내야★ 하기 때문이다. 체력 5 남은 적을 100 으로 때려도 흡혈은 5 다.
		//
		// 기본 공격이 아닌 피해(씨앗 폭탄·물대포 등)는 GetLifestealRatioForDamage 가 0 을 돌려준다.
		{
			const float LifestealRatio = UDRAbilitySystemLibrary::GetLifestealRatioForDamage(Props);
			if (LifestealRatio > 0.f)
			{
				const float ActuallyDealt = FMath::Min(LocalIncomingDamage, FMath::Max(0.f, GetHealth()));
				UDRAbilitySystemLibrary::ApplyHealToActor(
					Props.SourceAvatarActor,
					Props.SourceASC,
					UDRAbilitySystemLibrary::GetChipTuning(Props.SourceAvatarActor).Claw.HealEffect,
					ActuallyDealt * LifestealRatio);
			}
		}

		// ★Plan5(동전칩)★ 적중 회복 — 자판기 기본 공격이 적에게 ★맞을 때마다★ 고정량 회복.
		//
		// 흡혈과 같은 자리인 이유: "적에게 피해가 실제로 들어갔다"가 확정되는 지점이 여기다
		// (죽은 적은 PostGameplayEffectExecute 가, 0 피해는 위의 > 0 가드가 이미 걸렀다).
		// 피해량과 무관한 고정량이라 SetHealth 전후는 상관없지만, 튜토리얼 더미의 조기 return 보다는
		// 반드시 위여야 한다(흡혈과 똑같이 더미에서도 연습이 되게).
		//
		// 칩이 없거나 / 기본 공격이 아니거나 / 시전자가 오염 중이면 GetHitHealAmountForDamage 가 0 이다.
		// 만피·회복 차단이면 GetHealableAmount 가 0 이라 GE 자체를 걸지 않는다(적중이 잦아 헛 적용을 줄인다).
		{
			const float HitHeal = UDRAbilitySystemLibrary::GetHitHealAmountForDamage(Props);
			if (HitHeal > 0.f)
			{
				const float Healable = UDRAbilitySystemLibrary::GetHealableAmount(Props.SourceAvatarActor, HitHeal);
				if (Healable > 0.f)
				{
					UDRAbilitySystemLibrary::ApplyHealToActor(
						Props.SourceAvatarActor,
						Props.SourceASC,
						UDRAbilitySystemLibrary::GetChipTuning(Props.SourceAvatarActor).Coin.HealEffect,
						Healable);
				}
			}
		}

		// ★Plan5(공기탄칩)★ ② 공기탄이 적에게 들어갔다 — 흡착 1스택(보스 제외). 판정은 라이브러리가 한다 (D-97).
		// 더미 분기 · SetHealth 보다 위 — 튜토리얼 더미에도 걸린다(연습용)
		UDRAbilitySystemLibrary::TryApplyAirShotDrag(Props);

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

				// FirstAttacker가 없으면 설정 (부품 운반자도 공격 가능 — 도망 설계 폐기)
				if (bSourceIsPlayer && !BB->GetValueAsBool("HasFirstAttacker"))
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
			// 처치 크레딧: 막타를 친 플레이어 1명 (업적 판정 / 결과창 표시용, 재화 환산 없음)
			if (Props.SourceController)
			{
				if (ADRPlayerState* KillerPS = Props.SourceController->GetPlayerState<ADRPlayerState>())
				{
					KillerPS->AddStageKill();
				}
			}

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
