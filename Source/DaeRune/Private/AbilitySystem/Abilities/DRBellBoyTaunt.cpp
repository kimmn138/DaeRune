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
#include "Game/DRProgressionConfig.h"
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
	const float FPRate = UDRAbilitySystemLibrary::GetFirstPersonSyncRate(
		FirstPersonTauntMontage, FirstPersonSyncTag, TauntMontage, TauntStartEventTag);
	ActiveFirstPersonMontage = UDRAbilitySystemLibrary::PlayFirstPersonMontage(GetAvatarActorFromActorInfo(), FirstPersonTauntMontage, FPRate);
}

void UDRBellBoyTaunt::OnTauntStart(FGameplayEventData Payload)
{

	if (!HasAuthority(&CurrentActivationInfo)) return;   // 판정·GE 는 서버만

	// ★Plan5(도발칩)★ ② 반경 칩(SkillAreaRadius) — 스캔과 외침 큐가 같은 값을 쓴다. 칩이 없으면 TauntRadius 그대로 (Plan5 D-130)
	EffectiveTauntRadius = FMath::Max(0.f, GetUpgradedFloat(EDRUpgradeStat::SkillAreaRadius, TauntRadius));

	if (TauntStartCueTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			FGameplayCueParameters Params;
			Params.Location = GetAvatarActorFromActorInfo()->GetActorLocation();
			Params.RawMagnitude = EffectiveTauntRadius;
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
	UDRAbilitySystemLibrary::GetAliveEnemiesWithinRadius(Avatar, Avatar->GetActorLocation(), EffectiveTauntRadius, Enemies);

	for (AActor* Enemy : Enemies)
	{
		if (TauntedThisCast.Contains(Enemy)) continue;                     // 이번 시전에 이미 (D-15)
		if (!UDRAbilitySystemLibrary::CanBeTaunted(Enemy)) continue;       // 도주형 제외 (D-17)
		if (bRequireLineOfSight && !HasLineOfSight(Avatar, Enemy)) continue;

		ApplyTauntTo(Enemy);
		TauntedThisCast.Add(Enemy);
	}
}

void UDRBellBoyTaunt::ApplyTauntTo(AActor* Enemy)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy);
	if (!TargetASC || !TauntedEffect) return;

	const FDRGameplayTags& Tags = FDRGameplayTags::Get();

	// 마지막으로 건 벨보이가 이긴다 — 이전 도발(다른 벨보이 것 포함)을 먼저 지운다 (D-15)
	TargetASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(Tags.Debuff_Taunted));

	// 스펙 컨텍스트 = 이 어빌리티 → OriginalInstigatorASC = 벨보이 (도발자 식별 — D-13 / T-6)
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(TauntedEffect, GetAbilityLevel());
	if (!Spec.IsValid()) return;

	// ★Plan5(도발칩)★ ③ 적의 도발 시간만 바꾼다 — 벨보이 모션 5초는 그대로 (사용자 확정 — Plan5 D-131 · T-150)
	const float Duration = FMath::Max(0.1f, GetUpgradedFloat(EDRUpgradeStat::SkillDuration, TauntDuration));

	// ★Plan5(도발칩)★ ① 약점 노출 — 주는 피해 감소를 없애고 받는 피해를 늘린다. 배율은 이 스펙이 들고 다녀
	// 다른 벨보이가 덮어쓰면 그 벨보이의 칩 기준으로 바뀐다 (Plan8 D-15 · Plan5 D-129 · T-139)
	const bool bExpose = UDRAbilitySystemLibrary::HasUpgradeFeature(
		GetAbilitySystemComponentFromActorInfo(), Tags.Upgrade_Feature_Skill_Taunt_Expose);
	const float DamageTakenMult = bExpose
		? 1.f + FMath::Max(0.f, UDRAbilitySystemLibrary::GetChipTuning(Enemy).Taunt.ExposeDamageTakenBonus)
		: 1.f;

	Spec.Data->SetSetByCallerMagnitude(Tags.Data_Duration, Duration);                                          // 걸린 순간부터 5초(칩③ 3초)
	Spec.Data->SetSetByCallerMagnitude(Tags.Data_MoveSpeed_Mult, TauntedMoveSpeedMultiplier);                  // ×1.1
	Spec.Data->SetSetByCallerMagnitude(Tags.Data_Taunt_DamageMult, bExpose ? 1.f : TauntedDamageMultiplier);   // ×0.8(칩① 1.0)
	Spec.Data->SetSetByCallerMagnitude(Tags.Data_Taunt_DamageTakenMult, DamageTakenMult);                      // ×1.0(칩① 1.15)
	TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	// 지금 바로 타깃을 벨보이로 바꾸고, 도발이 끝날 때까지 0.2초마다 다시 쓴다 (D-16)
	if (const APawn* Pawn = Cast<APawn>(Enemy))
	{
		AAIController* AIC = Cast<AAIController>(Pawn->GetController());
		UDRAbilitySystemLibrary::ApplyTauntAggro(AIC);
		UDRAbilitySystemLibrary::StartTauntAggroRefresh(AIC);
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
