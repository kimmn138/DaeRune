// Copyright DaeRune

#include "AbilitySystem/Abilities/DRBellBoyBasicAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Actor/DRBellBoyHatPickup.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/DREnemy.h"
#include "DRGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Game/DRProgressionConfig.h"
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
		NextSwingIndex = 0;   // 첫 타는 항상 [0] 부터
	}

	UAnimMontage* Montage = SwingMontages.IsValidIndex(NextSwingIndex) ? SwingMontages[NextSwingIndex].Get() : nullptr;
	if (!Montage)
	{
		UE_LOG(LogDR, Warning, TEXT("[BellBoy] %s: SwingMontages[%d] 가 비어 있다"), *GetName(), NextSwingIndex);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const int32 SwingIndex = NextSwingIndex;   // FP 배열도 같은 인덱스 (Plan8 T-32)
	NextSwingIndex = (NextSwingIndex + 1) % SwingMontages.Num();

	// ── ② 선딜·후딜 → 재생 속도 (D-4) ──
	float HitTime = UDRAbilitySystemLibrary::FindMontageEventTime(Montage, HitEventTag);
	if (HitTime <= 0.f)
	{
		UE_LOG(LogDR, Warning, TEXT("[BellBoy] %s 에서 %s 노티파이를 못 찾음 — FallbackHitTime %.2f 사용"),
			*GetNameSafe(Montage), *HitEventTag.ToString(), FallbackHitTime);
		HitTime = FallbackHitTime;
	}

	// ★Plan5(가방칩)★ 칩 — 첫 타 선딜은 SkillWindup, 연속 선딜 · 후딜은 SkillComboWindup · SkillComboRecovery (Plan5 D-121).
	// 첫 타와 연속의 배율이 달라(가방칩① ×1.375 / ×1.5) 축이 둘이다. 칩이 없으면 전부 정확히 1.0
	const float WindupScale = FMath::Max(0.01f,
		GetUpgradedFloat(bCombo ? EDRUpgradeStat::SkillComboWindup : EDRUpgradeStat::SkillWindup, 1.f));
	const float RecoveryScale = bCombo
		? FMath::Max(0.01f, GetUpgradedFloat(EDRUpgradeStat::SkillComboRecovery, 1.f)) : 1.f;

	// ★Plan5(공속칩)★ 공속 배율 M — 실제 시간에서 한 번에 나눠 TP · FP 재생 속도 · 판정 창 · 종료 · 잠금이 같이 빨라진다.
	// 활성화 때 한 번만 읽는다 — 스윙 중 버프가 끝나도 이 스윙은 그대로 (Plan5 D-123 · T-152). 버프가 없으면 1.0
	const float SpeedMult = FMath::Max(0.01f, GetAttackSpeedMultiplier());
	const float WindupTime = (bCombo ? ComboWindupTime : FirstWindupTime) * WindupScale / SpeedMult;
	const float RecoveryTime = (bCombo ? ComboRecoveryTime : FirstRecoveryTime) * RecoveryScale / SpeedMult;

	// ★Plan5(가방칩)★ 끝 = 타격 + 실제 후딜, 판정 창은 그 안에서 끝난다 —
	// 서버만 늦게 끝나면 클라의 다음 예측 스윙이 거절된다 (Plan5 D-122 · T-134 · T-135)
	PendingRecoveryTime = FMath::Max(0.01f, RecoveryTime);
	ActiveHitWindowDuration = FMath::Min(HitWindowDuration, PendingRecoveryTime);

	const float WindupRate = HitTime / FMath::Max(0.01f, WindupTime);                                  // 0.4/0.4=1.0 · 0.4/0.2=2.0
	PendingRecoveryRate = FMath::Max(0.01f, Montage->GetPlayLength() - HitTime) / FMath::Max(0.01f, RecoveryTime); // 1.0 · 4.0

	// Plan8(FP) FP 는 자기 동기 프레임이 TP 타격과 같은 실시간(WindupTime)에 오도록 속도를 따로 준다 (D-36)
	UAnimMontage* FPMontage = FirstPersonSwingMontages.IsValidIndex(SwingIndex) ? FirstPersonSwingMontages[SwingIndex].Get() : nullptr;
	float FPWindupRate = WindupRate;
	PendingFirstPersonRecoveryRate = PendingRecoveryRate;
	if (FPMontage)
	{
		const float FPHitTime = UDRAbilitySystemLibrary::GetFirstPersonSyncTime(FPMontage, FirstPersonSyncTag, Montage, HitTime);
		FPWindupRate = FPHitTime / FMath::Max(0.01f, WindupTime);
		PendingFirstPersonRecoveryRate = FMath::Max(0.01f, FPMontage->GetPlayLength() - FPHitTime) / FMath::Max(0.01f, RecoveryTime);
	}

	// ── ③ 다른 스킬 차단 (선딜 + 0.4초) — 활성화 순간이라 예측 키가 살아 있어 클라 UI 도 즉시 막힌다 (D-7) ──
	if (SkillSwitchLockEffect)
	{
		FGameplayEffectSpecHandle LockSpec = MakeOutgoingGameplayEffectSpec(SkillSwitchLockEffect, GetAbilityLevel());
		if (LockSpec.IsValid())
		{
			// ★Plan5(공속칩)★ 전환 후딜도 공속을 받는다 — 선딜은 이미 ÷M (Plan5 D-123)
			LockSpec.Data->SetSetByCallerMagnitude(FDRGameplayTags::Get().Data_Duration,
				WindupTime + SkillSwitchRecoveryTime / SpeedMult);
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, LockSpec);
		}
	}

	// ── ④ 이번 스윙 상태 ──
	bHitEventReceived = false;
	bRecoveryDone = false;
	bHitWindowDone = false;
	HitActorsThisSwing.Reset();

	// ★Plan5(가방칩)★ ② 모자 낙하는 서버만 굴린다 — 활성화 때 1회 해석 (Plan5 D-126 · T-143)
	bHatDropThisSwing = HasAuthority(&ActivationInfo) && UDRAbilitySystemLibrary::HasUpgradeFeature(
		GetAbilitySystemComponentFromActorInfo(), FDRGameplayTags::Get().Upgrade_Feature_Skill_Bag_HatDrop);

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

	// Plan8(FP) 로컬 제어일 때만 FP 메시에 — 서버의 리모트 폰에서는 헬퍼가 건너뛴다 (D-33)
	ActiveFirstPersonMontage = UDRAbilitySystemLibrary::PlayFirstPersonMontage(GetAvatarActorFromActorInfo(), FPMontage, FPWindupRate);
}

void UDRBellBoyBasicAttack::OnHitEvent(FGameplayEventData Payload)
{
	bHitEventReceived = true;

	// ① 후딜 속도로 — 서버·소유 클라 모두 (D-4 / D-10)
	SetSwingPlayRate(PendingRecoveryRate, PendingFirstPersonRecoveryRate);

	// ★Plan5(가방칩)★ 끝 = 타격 + 실제 후딜 — 클라 · 서버가 같은 타이머로 끝난다.
	// 몽타주 블렌드 아웃 시작(끝 0.05초 전)에 끝내면 클라만 일찍 끝나 서버가 다음 스윙을 거절한다 (Plan5 D-122 · T-134)
	UAbilityTask_WaitDelay* RecoveryTask = UAbilityTask_WaitDelay::WaitDelay(this, PendingRecoveryTime);
	RecoveryTask->OnFinish.AddDynamic(this, &UDRBellBoyBasicAttack::OnRecoveryFinished);
	RecoveryTask->ReadyForActivation();

	// ② 판정은 서버만. 클라는 판정 창이 없는 것으로 친다
	if (!HasAuthority(&CurrentActivationInfo))
	{
		bHitWindowDone = true;
		TryFinishSwing();
		return;
	}

	if (ActiveHitWindowDuration <= 0.f || HitWindowSamples <= 1)
	{
		OnHitSample(0);   // 정원로봇 발톱과 같은 1회 판정
		bHitWindowDone = true;
		TryFinishSwing();
		return;
	}

	// 판정 창 0.1초면 0 / 0.025 / 0.05 / 0.075 초 — AbilityTask_Repeat 는 활성화 즉시 1회를 실행한다 (엔진 확인).
	// ★Plan5(가방칩)★ 창이 실제 후딜 안이라 마지막 샘플도 스윙이 끝나기 전에 나간다(가방칩③ · 공속이면 창이 줄어든다 — D-122).
	// 간격이 한 프레임보다 짧아도 루프 타이머가 밀린 만큼 연달아 실행해 샘플이 빠지지 않는다 (엔진 확인)
	UAbilityTask_Repeat* RepeatTask = UAbilityTask_Repeat::RepeatAction(
		this, ActiveHitWindowDuration / HitWindowSamples, HitWindowSamples);
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

		// ★Plan5(가방칩)★ ② 적 1마리 타격마다 모자 굴림 — 한 스윙에 대상당 1회는 HitActorsThisSwing 이 보장한다 (Plan5 D-126)
		TryDropHatOnEnemyHit(Target);
	}
}

void UDRBellBoyBasicAttack::TryDropHatOnEnemyHit(AActor* Target)
{
	// 적만 — 기본 공격은 클렌저 사이트에도 맞으므로 클래스로 거른다. 홀로그램 두더지도 굴리지 않는다 (Plan5 D-126 · T-144)
	if (!bHatDropThisSwing || !Cast<ADREnemy>(Target)) return;

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return;

	const FDRBagChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(Avatar).Bag;
	if (FMath::FRand() >= Tuning.HatDropChance) return;   // 적 1마리마다 10% (사용자 확정)

	UClass* PickupClass = Tuning.HatPickupClass.LoadSynchronous();
	if (!PickupClass)
	{
		UE_LOG(LogDR, Warning, TEXT("[BellBoy] ChipTuning.Bag.HatPickupClass 미지정 — 가방칩② 모자가 떨어지지 않는다"));
		return;
	}

	// 벨보이 발밑(사용자 확정) — 한 스윙에 여러 개가 떨어져도 겹쳐 안 보이게 살짝 흩뿌린다
	const FVector From = Avatar->GetActorLocation() + FVector(FMath::RandPointInCircle(Tuning.HatDropScatterRadius), 0.f);

	// 던진 사람 = 벨보이 → 본인이 주우면 2회(Plan8 D-23). 수명 · 2회 규칙은 벨보이의 모자칩을 따른다 (Plan5 D-128)
	ADRBellBoyHatPickup::SpawnOnGround(GetWorld(), PickupClass, From, FMath::FRandRange(0.f, 360.f),
		Avatar, GetAbilitySystemComponentFromActorInfo(), Avatar);
}

void UDRBellBoyBasicAttack::OnHitWindowFinished(int32 ActionNumber)
{
	bHitWindowDone = true;
	TryFinishSwing();
}

void UDRBellBoyBasicAttack::OnRecoveryFinished()
{
	bRecoveryDone = true;
	TryFinishSwing();
}

void UDRBellBoyBasicAttack::OnMontageFinished()
{
	// ★Plan5(가방칩)★ 타격을 받은 스윙의 끝은 후딜 타이머가 정한다 (Plan5 D-122) —
	// 블렌드 아웃 시작(끝 0.05초 전)에 끝내면 클라만 일찍 끝나 서버와 어긋난다 (Plan5 T-134)
	if (bHitEventReceived) return;

	// 노티파이가 없어서 타격 이벤트를 못 받았다 — 몽타주 끝으로 끝낸다. 안 그러면 영원히 안 끝난다 (Plan8 T-22 · Plan5 T-151)
	bHitWindowDone = true;
	bRecoveryDone = true;
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
	// ★Plan5(가방칩)★ 후딜 타이머(타격 + 실제 후딜)와 판정 창이 둘 다 끝나야 한다 — 판정 창은 후딜 안이라
	// 보통 후딜 타이머가 마지막이고, 클라 · 서버가 같은 시각에 끝난다 (Plan5 D-122)
	if (bRecoveryDone && bHitWindowDone)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UDRBellBoyBasicAttack::SetSwingPlayRate(float NewRate, float FPRate)
{
	// Plan8(FP) 리슨 호스트 자기 폰은 서버이면서 FP 도 보므로 TP 분기와 무관하게 먼저 (D-36 / T-37)
	UDRAbilitySystemLibrary::SetFirstPersonMontagePlayRate(GetAvatarActorFromActorInfo(), ActiveFirstPersonMontage, FPRate);

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
}

void UDRBellBoyBasicAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!bWasCancelled)
	{
		LastSwingEndTime = GetWorld()->GetTimeSeconds();   // 연속 판정 기준 (D-6)
	}
	else
	{
		// Plan8(FP) 끊기면(피격·서버 거절) FP 스윙도 멈춘다. 정상 종료면 다음 스윙이 같은 슬롯에서 밀어낸다 (D-37)
		UDRAbilitySystemLibrary::StopFirstPersonMontage(GetAvatarActorFromActorInfo(), ActiveFirstPersonMontage);
	}
	ActiveFirstPersonMontage = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
