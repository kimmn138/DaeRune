// Copyright DaeRune

#include "AbilitySystem/Abilities/DRBellBoyBasicAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "DRGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Interaction/CombatInterface.h"
#include "DaeRune/DRLogChannels.h"

UDRBellBoyBasicAttack::UDRBellBoyBasicAttack()
{
	// 콤보 상태(LastSwingEndTime / NextSwingIndex)를 인스턴스에 둔다
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UDRBellBoyBasicAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ── ① 첫 타인가, 연속인가 (D-6) ──
	const bool bCombo = (GetWorld()->GetTimeSeconds() - LastSwingEndTime) <= ComboResetTime;
	if (!bCombo)
	{
		NextSwingIndex = 0;   // 첫 타는 항상 오른쪽부터
	}

	UAnimMontage* Montage = SwingMontages.IsValidIndex(NextSwingIndex) ? SwingMontages[NextSwingIndex].Get() : nullptr;
	if (!Montage)
	{
		UE_LOG(LogDR, Warning, TEXT("[BellBoy] %s: SwingMontages[%d] 가 비어 있다"), *GetName(), NextSwingIndex);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	NextSwingIndex = (NextSwingIndex + 1) % SwingMontages.Num();

	// ── ② 선딜·후딜 → 재생 속도 (D-4) ──
	float HitTime = UDRAbilitySystemLibrary::FindMontageEventTime(Montage, HitEventTag);
	if (HitTime <= 0.f)
	{
		UE_LOG(LogDR, Warning, TEXT("[BellBoy] %s 에서 %s 노티파이를 못 찾음 — FallbackHitTime %.2f 사용"),
			*GetNameSafe(Montage), *HitEventTag.ToString(), FallbackHitTime);
		HitTime = FallbackHitTime;
	}

	// 칩(SkillWindup) — 칩이 없으면 정확히 1.0. 나중에 벨보이 칩이 생겨도 그대로 동작한다.
	const float WindupScale = FMath::Max(0.01f, GetUpgradedFloat(EDRUpgradeStat::SkillWindup, 1.f));
	const float WindupTime = (bCombo ? ComboWindupTime : FirstWindupTime) * WindupScale;
	const float RecoveryTime = bCombo ? ComboRecoveryTime : FirstRecoveryTime;

	const float WindupRate = HitTime / FMath::Max(0.01f, WindupTime);                                  // 0.4/0.4=1.0 · 0.4/0.2=2.0
	PendingRecoveryRate = FMath::Max(0.01f, Montage->GetPlayLength() - HitTime) / FMath::Max(0.01f, RecoveryTime); // 1.0 · 4.0

	// ── ③ 다른 스킬 차단 (선딜 + 0.4초) — 활성화 순간이라 예측 키가 살아 있어 클라 UI 도 즉시 막힌다 (D-7) ──
	if (SkillSwitchLockEffect)
	{
		FGameplayEffectSpecHandle LockSpec = MakeOutgoingGameplayEffectSpec(SkillSwitchLockEffect, GetAbilityLevel());
		if (LockSpec.IsValid())
		{
			LockSpec.Data->SetSetByCallerMagnitude(FDRGameplayTags::Get().Data_Duration, WindupTime + SkillSwitchRecoveryTime);
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, LockSpec);
		}
	}

	// ── ④ 이번 스윙 상태 ──
	bHitEventReceived = false;
	bMontageDone = false;
	bHitWindowDone = false;
	HitActorsThisSwing.Reset();

	// ── ⑤ 타격 이벤트를 먼저 기다리고, 몽타주를 튼다 ──
	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, HitEventTag, nullptr, /*OnlyTriggerOnce*/ true);
	HitTask->EventReceived.AddDynamic(this, &UDRBellBoyBasicAttack::OnHitEvent);
	HitTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, WindupRate);
	MontageTask->OnCompleted.AddDynamic(this, &UDRBellBoyBasicAttack::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &UDRBellBoyBasicAttack::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UDRBellBoyBasicAttack::OnMontageAborted);
	MontageTask->OnCancelled.AddDynamic(this, &UDRBellBoyBasicAttack::OnMontageAborted);
	MontageTask->ReadyForActivation();

	// (STEP 11) FP 에셋이 오면 여기서 FirstPersonMesh 에 같은 속도로 1P 몽타주를 튼다
}

void UDRBellBoyBasicAttack::OnHitEvent(FGameplayEventData Payload)
{
	bHitEventReceived = true;

	// ① 후딜 속도로 — 서버·소유 클라 모두 (D-4 / D-10)
	SetSwingPlayRate(PendingRecoveryRate);

	// ② 판정은 서버만. 클라는 판정 창이 없는 것으로 친다
	if (!HasAuthority(&CurrentActivationInfo))
	{
		bHitWindowDone = true;
		TryFinishSwing();
		return;
	}

	if (HitWindowDuration <= 0.f || HitWindowSamples <= 1)
	{
		OnHitSample(0);   // 정원로봇 발톱과 같은 1회 판정
		bHitWindowDone = true;
		TryFinishSwing();
		return;
	}

	// 0 / 0.025 / 0.05 / 0.075 초 — AbilityTask_Repeat 는 활성화 즉시 1회를 실행한다 (엔진 확인)
	UAbilityTask_Repeat* RepeatTask = UAbilityTask_Repeat::RepeatAction(
		this, HitWindowDuration / HitWindowSamples, HitWindowSamples);
	RepeatTask->OnPerformAction.AddDynamic(this, &UDRBellBoyBasicAttack::OnHitSample);
	RepeatTask->OnFinished.AddDynamic(this, &UDRBellBoyBasicAttack::OnHitWindowFinished);
	RepeatTask->ReadyForActivation();
}

void UDRBellBoyBasicAttack::OnHitSample(int32 SampleIndex)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return;

	const FVector Center = ICombatInterface::Execute_GetCombatSocketLocation(Avatar, HitSocketTag);
	const float Radius = GetUpgradedAttackRadius();   // 범위 칩(SkillAreaRadius) 반영

	if (bDrawDebugHit)
	{
		DrawDebugSphere(GetWorld(), Center, Radius, 12, FColor::Orange, false, 0.5f);
	}

	// 대상 수집은 정원로봇 발톱과 같은 함수 — 홀로그램 두더지·클렌저 사이트 처리까지 같게 (D-32)
	TArray<AActor*> Targets;
	UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(Avatar, Targets, TArray<AActor*>{ Avatar }, Radius, Center);

	for (AActor* Target : Targets)
	{
		if (!IsValid(Target) || HitActorsThisSwing.Contains(Target)) continue;   // 한 스윙에 한 대상 1회
		if (!UDRAbilitySystemLibrary::IsValidDamageTarget(Avatar, Target)) continue;

		HitActorsThisSwing.Add(Target);
		// 피해 30 + 넉백(KnockbackForceMagnitude, 45° 위) — 기존 파이프라인 그대로 (D-9)
		UDRAbilitySystemLibrary::ApplyDamageEffect(MakeDamageEffectParamsFromClassDefaults(Target));
	}
}

void UDRBellBoyBasicAttack::OnHitWindowFinished(int32 ActionNumber)
{
	bHitWindowDone = true;
	TryFinishSwing();
}

void UDRBellBoyBasicAttack::OnMontageFinished()
{
	bMontageDone = true;

	// 노티파이가 없어서 타격 이벤트를 못 받았으면 판정 창도 끝난 것으로 — 안 그러면 영원히 안 끝난다 (T-22)
	if (!bHitEventReceived)
	{
		bHitWindowDone = true;
	}
	TryFinishSwing();
}

void UDRBellBoyBasicAttack::OnMontageAborted()
{
	// 피격·스턴으로 끊김 → 콤보도 끊는다 (다음 스윙은 첫 타)
	LastSwingEndTime = -1000.0;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UDRBellBoyBasicAttack::TryFinishSwing()
{
	// 연속 공격은 후딜(0.1초)과 판정 창(0.1초)이 거의 같이 끝난다 — 둘 다 기다려야 마지막 샘플이 잘리지 않는다
	if (bMontageDone && bHitWindowDone)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UDRBellBoyBasicAttack::SetSwingPlayRate(float NewRate)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	if (ASC->IsOwnerActorAuthoritative())
	{
		// 서버: 로컬 재생 속도 + 시뮬레이티드 프록시용 복제 데이터 갱신 (엔진 CurrentMontageSetPlayRate)
		ASC->CurrentMontageSetPlayRate(NewRate);
		return;
	}

	// 소유 클라: 로컬만. (CurrentMontageSetPlayRate 를 쓰면 서버로 RPC 가 한 번 더 간다)
	if (const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo())
	{
		if (UAnimInstance* AnimInstance = Info->GetAnimInstance())
		{
			if (UAnimMontage* ActiveMontage = AnimInstance->GetCurrentActiveMontage())
			{
				AnimInstance->Montage_SetPlayRate(ActiveMontage, NewRate);
			}
		}
	}
	// (STEP 11) FP 메시 몽타주도 같은 속도로
}

void UDRBellBoyBasicAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!bWasCancelled)
	{
		LastSwingEndTime = GetWorld()->GetTimeSeconds();   // 연속 판정 기준 (D-6)
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
