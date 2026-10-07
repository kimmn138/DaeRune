// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "DRVendingMachineAttackSpeedBuff.generated.h"

/**
 * 자판기 로봇 공격속도 버프 스킬 GA (E 키)
 * Water 50 을 소모하고 공격속도 버프를 1스택 건다. 칩이 없으면 스택당 +20%, 최대 10중첩, 지속 20초(사용 시 리셋).
 * 스택당 증가량은 ChipTuning.AttackSpeedBuff.BaseSpeedPerStack, 최대 중첩은 GE 에셋의 Stack Limit 이 권위다.
 * 실제 ActivateAbility 흐름은 Blueprint 에 있고, 버프 적용은 ApplyAttackSpeedBuff() 를 부른다 (Plan5(공속칩) D-65).
 */
UCLASS()
class DAERUNE_API UDRVendingMachineAttackSpeedBuff : public UDRGameplayAbility
{
	GENERATED_BODY()

public:
	/**
	 * ★Plan5(공속칩)★ 버프를 건다 (서버에서만 동작, 클라에서 불리면 아무것도 안 한다).
	 * BP 의 MakeOutgoingSpec + ApplyGameplayEffectSpecToSelf 두 노드를 이 한 노드로 바꾼다.
	 *  - 칩 ② Share  : 본인 + SharedRadius 안 살아 있는 아군에게 SharedEffect (지속 SharedDuration)
	 *  - 칩 ③ Decay  : 자기에게 DecayEffect (지속 DecayInterval) + 첫 만료를 BuffDuration 뒤로 민다 (D-73)
	 *  - 그 외(①·없음): 자기에게 AttackSpeedBuffEffect (지속 ①이면 OverdriveDuration, 아니면 BuffDuration)
	 * 지속시간은 이 함수가 스펙에 고정해 넣는다 — GE 에셋의 Duration 설정(고정값/SetByCaller)과 무관하다.
	 * ②·③의 GE 가 비어 있으면 경고 로그를 남기고 기본 버프로 동작한다(물은 이미 냈으므로).
	 */
	UFUNCTION(BlueprintCallable, Category = "VendingMachine|Skill")
	void ApplyAttackSpeedBuff();

	/** BP에서 ApplyAttackSpeedBuff 직후 호출. 서버 권한일 때만 현재 버프 스택(자기 + 공유)을 조회해
	    캐릭터의 MulticastPlayVendingSkillUse(Stacks)를 트리거한다 (Plan2.md §3.3). */
	UFUNCTION(BlueprintCallable, Category = "VendingMachine|Skill")
	void NotifySkillActivated();

	float GetBuffDuration() const { return BuffDuration; }

	/**
	 * ★Plan5(공속칩)★ 이 ASC 에 부여된 공속 버프 GA 의 BuffDuration(칩 ③의 유지 시간). 못 찾으면 C++ 기본값.
	 * 칩 ③ GE 는 Spec 지속이 1초라 그래프의 "전체 시간"을 GE 에서 읽을 수 없다 — HUD 가 여기서 읽는다.
	 * 부여된 어빌리티 목록은 소유 클라에도 복제되므로 클라에서 불러도 된다.
	 */
	static float FindBuffDuration(const UAbilitySystemComponent* ASC);

protected:
	/** 칩이 없을 때(및 칩 ①이 아닐 때) 적용할 공격속도 버프 GE 클래스 (BP에서 설정) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Skill")
	TSubclassOf<UGameplayEffect> AttackSpeedBuffEffect;

	/** ★Plan5(공속칩)★ 칩이 없을 때 자기 버프 지속(초). 칩 ③은 이 시간만큼 유지한 뒤 1스택씩 줄어든다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Skill", meta = (ClampMin = "0.1"))
	float BuffDuration = 20.f;

private:
	/** Target 에게 EffectClass 를 Duration(초) 고정으로 건다. 서버 전용. 스택이 쌓이면 기존 핸들을 돌려준다 */
	FActiveGameplayEffectHandle ApplyBuffEffect(AActor* Target, TSubclassOf<UGameplayEffect> EffectClass, float Duration) const;
};
