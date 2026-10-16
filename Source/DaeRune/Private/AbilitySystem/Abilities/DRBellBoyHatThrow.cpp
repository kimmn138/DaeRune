// Copyright DaeRune

#include "AbilitySystem/Abilities/DRBellBoyHatThrow.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Character/DRCharacter.h"
#include "Interaction/CombatInterface.h"

UDRBellBoyHatThrow::UDRBellBoyHatThrow()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NumProjectiles = 1;   // 부모 기본값은 5 — 모자는 한 개다
}

void UDRBellBoyHatThrow::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))   // 물 20
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_WaitGameplayEvent* ReleaseTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, ReleaseEventTag, nullptr, /*OnlyTriggerOnce*/ true);
	ReleaseTask->EventReceived.AddDynamic(this, &UDRBellBoyHatThrow::OnRelease);
	ReleaseTask->ReadyForActivation();

	// ★Plan5(공속칩)★ 공속 배율 M — TP · FP 던지기가 같이 빨라진다(릴리스 = 모자 스폰도 1/M 시각).
	// 버프가 없으면 1.0 이라 예전과 같다 (Plan5 D-124)
	const float SpeedMult = FMath::Max(0.01f, GetAttackSpeedMultiplier());

	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ThrowMontage, SpeedMult);
	MontageTask->OnCompleted.AddDynamic(this, &UDRBellBoyHatThrow::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &UDRBellBoyHatThrow::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UDRBellBoyHatThrow::OnMontageAborted);
	MontageTask->OnCancelled.AddDynamic(this, &UDRBellBoyHatThrow::OnMontageAborted);
	MontageTask->ReadyForActivation();

	// Plan8(FP) FP 릴리스 프레임이 TP 릴리스(=모자 스폰)와 같은 순간에 오도록 (D-35). 아래 FPRate 는 TP 속도 1 기준
	float FPRate = 1.f;
	const float TPEventTime = UDRAbilitySystemLibrary::FindMontageEventTime(ThrowMontage, ReleaseEventTag);
	if (FirstPersonThrowMontage && TPEventTime > 0.f)
	{
		FPRate = UDRAbilitySystemLibrary::GetFirstPersonSyncTime(FirstPersonThrowMontage, FirstPersonSyncTag, ThrowMontage, TPEventTime) / TPEventTime;
	}
	// ★Plan5(공속칩)★ TP 가 M 배속이므로 FP 도 같은 M 을 곱해야 릴리스 프레임이 계속 맞는다 (Plan5 D-124 · T-137)
	ActiveFirstPersonMontage = UDRAbilitySystemLibrary::PlayFirstPersonMontage(GetAvatarActorFromActorInfo(), FirstPersonThrowMontage, FPRate * SpeedMult);
}

void UDRBellBoyHatThrow::OnRelease(FGameplayEventData Payload)
{
	// 스폰은 서버만 (SpawnProjectile 안에서도 확인한다). 조준점 = 크로스헤어(카메라 트레이스)
	if (!HasAuthority(&CurrentActivationInfo)) return;
	SpawnProjectileAt(GetReleaseLocation(), CalculateTargetLocation());
}

FVector UDRBellBoyHatThrow::GetReleaseLocation() const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();

	// Plan8(FP) FP 메시의 고정 소켓 — 서버도 같은 위치를 읽는다(청소기 FP 총구와 같은 방식, D-39)
	if (!FirstPersonReleaseSocketName.IsNone())
	{
		const ADRCharacter* DRChar = Cast<ADRCharacter>(Avatar);
		if (DRChar && DRChar->FirstPersonMesh && DRChar->FirstPersonMesh->DoesSocketExist(FirstPersonReleaseSocketName))
		{
			return DRChar->FirstPersonMesh->GetSocketLocation(FirstPersonReleaseSocketName);
		}
	}
	return ICombatInterface::Execute_GetCombatSocketLocation(Avatar, ThrowSocketTag);
}

void UDRBellBoyHatThrow::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UDRBellBoyHatThrow::OnMontageAborted()
{
	// 릴리스 전에 끊기면 모자는 안 나간다 — 물은 이미 썼다(다른 스킬과 같은 규칙)
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UDRBellBoyHatThrow::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Plan8(FP) 끊기면 FP 던지기도 멈춘다 — 손의 모자 소품은 노티파이 스테이트 End 가 치운다 (D-37 / T-35)
	if (bWasCancelled)
	{
		UDRAbilitySystemLibrary::StopFirstPersonMontage(GetAvatarActorFromActorInfo(), ActiveFirstPersonMontage);
	}
	ActiveFirstPersonMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
