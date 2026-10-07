// Copyright DaeRune

#include "AbilitySystem/Abilities/DRBellBoyHatThrow.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

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

	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ThrowMontage, 1.f);
	MontageTask->OnCompleted.AddDynamic(this, &UDRBellBoyHatThrow::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &UDRBellBoyHatThrow::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UDRBellBoyHatThrow::OnMontageAborted);
	MontageTask->OnCancelled.AddDynamic(this, &UDRBellBoyHatThrow::OnMontageAborted);
	MontageTask->ReadyForActivation();
}

void UDRBellBoyHatThrow::OnRelease(FGameplayEventData Payload)
{
	// 스폰은 서버만 (SpawnProjectile 안에서도 확인한다). 조준점 = 크로스헤어(카메라 트레이스)
	if (!HasAuthority(&CurrentActivationInfo)) return;
	SpawnProjectile(CalculateTargetLocation(), ThrowSocketTag);
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
