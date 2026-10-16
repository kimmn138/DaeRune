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
 * ★Plan5(공속칩)★ 공속 배율 M 만큼 TP · FP 던지기가 빨라진다 — 릴리스(모자 스폰)도 1/M 시각 (Plan5 D-124).
 * ★Plan5(모자칩)★ 칩 효과(아군 피해 · 2회 · 바닥 수명)는 투사체 · 픽업이 던진 벨보이의 칩으로 해석한다 (Plan5 D-128 · D-133 · D-134)
 */
UCLASS()
class DAERUNE_API UDRBellBoyHatThrow : public UDRProjectileSpell
{
	GENERATED_BODY()

public:
	UDRBellBoyHatThrow();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

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

	// ========== ★Plan8(FP)★ 1인칭 ==========

	/** FP 던지기 몽타주(FP_ThrowCap, DefaultSlot — 손의 모자는 Attach Prop 노티파이 스테이트). 비우면 FP 재생 없음. ★ReleaseEventTag 노티파이 금지★ (Plan8 T-30) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|FirstPerson")
	TObjectPtr<UAnimMontage> FirstPersonThrowMontage;

	/** FP 몽타주에서 "TP 릴리스와 같은 순간"을 표시하는 노티파이 태그 (Event.Montage.BellBoy.FPSync). 없으면 길이 비례 (Plan8 D-35) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|FirstPerson")
	FGameplayTag FirstPersonSyncTag;

	/**
	 * 모자를 내보낼 FirstPersonMesh 소켓(CapReleaseSocket). 비우거나 소켓이 없으면 ThrowSocketTag(TP CapSocket).
	 * ★루트 본에 붙은 고정 소켓★ 이어야 한다 — 서버엔 FP 애니가 없어 손 본 소켓은 클라와 위치가 갈린다 (Plan8 D-39 / T-36)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|FirstPerson")
	FName FirstPersonReleaseSocketName;

private:
	UFUNCTION()
	void OnRelease(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageAborted();

	// 모자가 나갈 위치 — FP 고정 소켓이 있으면 그것, 없으면 TP 소켓 (Plan8 D-39)
	FVector GetReleaseLocation() const;

	TObjectPtr<UAnimMontage> ActiveFirstPersonMontage;   // 로컬 제어일 때만 채워진다 (Plan8 D-33)
};
