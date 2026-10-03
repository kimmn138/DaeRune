// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRProjectileSpell.h"
#include "DRBellBoyHatThrow.generated.h"

class UAnimMontage;

/**
 * ★Plan8(모자)★ 벨보이 스킬2 — 모자 던지기 (RMB).
 * 릴리스 노티파이에서 BP_BellBoyHatProjectile 을 조준점으로 스폰한다. 이후 판정은 전부 투사체·픽업이 한다.
 * 피해(30)·소스 ASC 는 부모 SpawnProjectile 이 MakeDamageEffectParamsFromClassDefaults() 로 넣어 준다.
 */
UCLASS()
class DAERUNE_API UDRBellBoyHatThrow : public UDRProjectileSpell
{
	GENERATED_BODY()

public:
	UDRBellBoyHatThrow();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 던지기 모션 (BB_ThrowCap 0.8초, UpperBody) — 릴리스 노티파이 + ANS_BellBoy_HideCap */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|HatThrow")
	TObjectPtr<UAnimMontage> ThrowMontage;

	/** 모자가 손을 떠나는 순간 (Event.Montage.BellBoy.HatThrow) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|HatThrow")
	FGameplayTag ReleaseEventTag;

	/** 모자가 나가는 위치 — CombatSocket.RightHand → BP_BellBoy 의 RightHandSocketName(CapSocket) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|HatThrow")
	FGameplayTag ThrowSocketTag;

private:
	UFUNCTION()
	void OnRelease(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageAborted();
};
