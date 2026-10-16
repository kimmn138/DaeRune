// Copyright DaeRune

#include "AbilitySystem/Abilities/DRBellBoyTaunt.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "DRGameplayTags.h"
#include "DaeRune/DRLogChannels.h"

UDRBellBoyTaunt::UDRBellBoyTaunt()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UDRBellBoyTaunt::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))   // 물 60 (WaterCost + GE_Cost)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	TauntedThisCast.Reset();

	// [임시 진단]
	UE_LOG(LogDR, Warning, TEXT("[TauntDiag] 1. 발동 %s — 서버=%d 몽타주=%s 시작이벤트=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), HasAuthority(&ActivationInfo) ? 1 : 0,
		*GetNameSafe(TauntMontage), *TauntStartEventTag.ToString());

	UAbilityTask_WaitGameplayEvent* StartTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, TauntStartEventTag, nullptr, /*OnlyTriggerOnce*/ true);
	StartTask->EventReceived.AddDynamic(this, &UDRBellBoyTaunt::OnTauntStart);
	StartTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, TauntMontage, 1.f);
	MontageTask->OnCompleted.AddDynamic(this, &UDRBellBoyTaunt::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &UDRBellBoyTaunt::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UDRBellBoyTaunt::OnMontageAborted);
	MontageTask->OnCancelled.AddDynamic(this, &UDRBellBoyTaunt::OnMontageAborted);
	MontageTask->ReadyForActivation();

	// Plan8(FP) FP 포즈 프레임이 TP 판정 시작과 같은 순간에 오도록 (D-35). TP 는 속도 1
	float FPRate = 1.f;
	const float TPEventTime = UDRAbilitySystemLibrary::FindMontageEventTime(TauntMontage, TauntStartEventTag);
	if (FirstPersonTauntMontage && TPEventTime > 0.f)
	{
		FPRate = UDRAbilitySystemLibrary::GetFirstPersonSyncTime(FirstPersonTauntMontage, FirstPersonSyncTag, TauntMontage, TPEventTime) / TPEventTime;
	}
	ActiveFirstPersonMontage = UDRAbilitySystemLibrary::PlayFirstPersonMontage(GetAvatarActorFromActorInfo(), FirstPersonTauntMontage, FPRate);
}

void UDRBellBoyTaunt::OnTauntStart(FGameplayEventData Payload)
{
	// [임시 진단] 서버=0 줄만 있고 서버=1 줄이 없으면 → 서버에서 노티파이가 안 불린 것
	UE_LOG(LogDR, Warning, TEXT("[TauntDiag] 2. 판정 시작 이벤트 받음 — 서버=%d"), HasAuthority(&CurrentActivationInfo) ? 1 : 0);

	if (!HasAuthority(&CurrentActivationInfo)) return;   // 판정·GE 는 서버만

	if (TauntStartCueTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			FGameplayCueParameters Params;
			Params.Location = GetAvatarActorFromActorInfo()->GetActorLocation();
			Params.RawMagnitude = TauntRadius;
			ASC->ExecuteGameplayCue(TauntStartCueTag, Params);
		}
	}

	// 0.0 / 0.1 / … / 0.5 초 → 6회 (Repeat 는 활성화 즉시 1회를 실행한다)
	const int32 ScanCount = FMath::Max(1, FMath::FloorToInt(DetectWindow / DetectInterval + KINDA_SMALL_NUMBER) + 1);
	UAbilityTask_Repeat* ScanTask = UAbilityTask_Repeat::RepeatAction(this, DetectInterval, ScanCount);
	ScanTask->OnPerformAction.AddDynamic(this, &UDRBellBoyTaunt::OnScan);
	ScanTask->ReadyForActivation();
}

void UDRBellBoyTaunt::OnScan(int32 ActionNumber)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return;

	TArray<AActor*> Enemies;
	UDRAbilitySystemLibrary::GetAliveEnemiesWithinRadius(Avatar, Avatar->GetActorLocation(), TauntRadius, Enemies);

	// [임시 진단]
	UE_LOG(LogDR, Warning, TEXT("[TauntDiag] 3. 스캔 #%d — 반경 %.0f 안의 살아 있는 적 %d"), ActionNumber, TauntRadius, Enemies.Num());

	for (AActor* Enemy : Enemies)
	{
		if (TauntedThisCast.Contains(Enemy)) continue;                     // 이번 시전에 이미 (D-15)
		if (!UDRAbilitySystemLibrary::CanBeTaunted(Enemy))                 // 도주형 제외 (D-17)
		{
			UE_LOG(LogDR, Warning, TEXT("[TauntDiag] 3-x. %s 제외 — 블랙보드에 FirstAttacker/TargetToFollow 키 없음(또는 AI 컨트롤러 없음)"), *GetNameSafe(Enemy));
			continue;
		}
		if (bRequireLineOfSight && !HasLineOfSight(Avatar, Enemy)) continue;

		ApplyTauntTo(Enemy);
		TauntedThisCast.Add(Enemy);
	}
}

void UDRBellBoyTaunt::ApplyTauntTo(AActor* Enemy)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy);
	if (!TargetASC || !TauntedEffect)
	{
		UE_LOG(LogDR, Warning, TEXT("[TauntDiag] 3-x. %s 도발 실패 — ASC=%d TauntedEffect=%s"),
			*GetNameSafe(Enemy), TargetASC ? 1 : 0, *GetNameSafe(TauntedEffect.Get()));   // [임시 진단]
		return;
	}

	const FDRGameplayTags& Tags = FDRGameplayTags::Get();

	// 마지막으로 건 벨보이가 이긴다 — 이전 도발(다른 벨보이 것 포함)을 먼저 지운다 (D-15)
	TargetASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(Tags.Debuff_Taunted));

	// 스펙 컨텍스트 = 이 어빌리티 → OriginalInstigatorASC = 벨보이 (도발자 식별 — D-13 / T-6)
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(TauntedEffect, GetAbilityLevel());
	if (!Spec.IsValid()) return;

	Spec.Data->SetSetByCallerMagnitude(Tags.Data_Duration, TauntDuration);                       // 걸린 순간부터 5초
	Spec.Data->SetSetByCallerMagnitude(Tags.Data_MoveSpeed_Mult, TauntedMoveSpeedMultiplier);   // ×1.1
	Spec.Data->SetSetByCallerMagnitude(Tags.Data_Taunt_DamageMult, TauntedDamageMultiplier);    // ×0.8
	const FActiveGameplayEffectHandle Applied = TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	// 지금 바로 타깃을 벨보이로 바꾸고, 도발이 끝날 때까지 0.2초마다 다시 쓴다 (D-16)
	if (const APawn* Pawn = Cast<APawn>(Enemy))
	{
		AAIController* AIC = Cast<AAIController>(Pawn->GetController());
		const bool bAggro = UDRAbilitySystemLibrary::ApplyTauntAggro(AIC);
		UDRAbilitySystemLibrary::StartTauntAggroRefresh(AIC);

		// [임시 진단] GE=0 이면 GE 적용 실패, Taunted 태그=0 이면 GE 에 Debuff.Taunted 가 안 붙음, 어그로=0 이면 ApplyTauntAggro 가 거부
		UE_LOG(LogDR, Warning, TEXT("[TauntDiag] 3-o. %s 도발 — GE=%d Taunted태그=%d AI=%s 어그로=%d"),
			*GetNameSafe(Enemy), Applied.IsValid() ? 1 : 0,
			TargetASC->HasMatchingGameplayTag(Tags.Debuff_Taunted) ? 1 : 0, *GetNameSafe(AIC), bAggro ? 1 : 0);
	}
}

bool UDRBellBoyTaunt::HasLineOfSight(const AActor* From, const AActor* To) const
{
	// ★월드 지형만★ 본다 — Visibility 채널로 쏘면 사이에 선 다른 적의 히트박스가 시야를 막는다 (T-17 과 같은 이유)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BellBoyTauntLOS), false, From);
	Params.AddIgnoredActor(To);
	return !GetWorld()->LineTraceTestByObjectType(From->GetActorLocation(), To->GetActorLocation(),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

void UDRBellBoyTaunt::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);   // 도발은 적마다 남는다
}

void UDRBellBoyTaunt::OnMontageAborted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UDRBellBoyTaunt::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Plan8(FP) 끊기면(서버 거절 등) FP 도발 모션도 멈춘다 (D-37)
	if (bWasCancelled)
	{
		UDRAbilitySystemLibrary::StopFirstPersonMontage(GetAvatarActorFromActorInfo(), ActiveFirstPersonMontage);
	}
	ActiveFirstPersonMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
