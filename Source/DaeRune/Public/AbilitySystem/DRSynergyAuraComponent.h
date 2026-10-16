// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "DRSynergyAuraComponent.generated.h"

class UGameplayEffect;

/**
 * 칩 CM.Asc.Synergy 전용 근접 시너지 오라. ★서버 전용★ 으로 동작한다.
 * (Plan3(공용칩) §17.7)
 *
 * 동작:
 *   타이머 → 반경 내 최근접 팀원 1명 탐지 → {자신, 그 팀원} 에게 GE 부여
 *   팀원이 없으면 ★자신 것까지 해제★ 한다(요구사항 명시).
 *
 * ★수치(10%)를 이 컴포넌트가 들고 있지 않다.★
 * GE 는 Buff.Upgrade.Synergy 태그만 부여하고, 실제 증감은
 * UExecCalc_Damage(주는 피해) / UDRPlayerAttributeSet::HandleIncomingDamage(받는 피해) 가
 * ASC 의 ★태그 카운트★ 로 계산한다. 그래야 여러 명이 겹쳐 걸었을 때
 * 중첩이 자료구조만으로 성립한다. (§17.7.3)
 *
 * 칩이 없으면 타이머가 아예 돌지 않아 비용이 0 이다.
 */
UCLASS(ClassGroup = (DaeRune), meta = (BlueprintSpawnableComponent))
class DAERUNE_API UDRSynergyAuraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRSynergyAuraComponent();

	/**
	 * 칩 장착 상태에 맞춰 오라를 켜고 끈다.
	 * ADRCharacter::RefreshUpgradeEffects() 에서 호출한다 —
	 * 그 함수는 모든 머신에서 불리지만 이 함수가 내부에서 서버 권한을 검사한다.
	 */
	void RefreshFromUpgrade();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void TickAura();

	/** 반경 내 살아 있는 다른 플레이어 중 최근접 1명. 없으면 nullptr. */
	AActor* FindNearestTeammate(const FVector& Origin, float Radius) const;

	void ApplyBuffTo(AActor* Target);

	/** 대상이 이미 파괴됐어도 안전하게 동작한다. */
	void RemoveBuffFrom(const TWeakObjectPtr<AActor>& TargetKey, FActiveGameplayEffectHandle Handle);

	/** 내가 건 버프를 전부 회수한다 (칩 해제 / 사망 / EndPlay). */
	void ClearAllBuffs();

	/** 대상 → 내가 그 대상에게 건 GE 핸들. 같은 대상에 중복 적용되는 것을 막는다. */
	TMap<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle> AppliedBuffs;

	FTimerHandle AuraTickTimerHandle;

	/** Config 의 TSoftClassPtr 를 1회 동기 로드해 캐시한다. */
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> CachedBuffEffectClass;

	/** 미지정 경고를 매 틱 쏟아내지 않기 위한 1회성 플래그 */
	bool bLoggedMissingEffect = false;
};
