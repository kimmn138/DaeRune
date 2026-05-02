// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "DRVendingMachineAttackSpeedBuff.generated.h"

/**
 * 자판기 로봇 공격속도 버프 스킬 GA
 * Q 입력 시 Water 50을 소모하고 공격속도 +10% 버프를 적용한다.
 * 중첩 가능, 지속시간 10초 (중첩 시 리셋).
 * 실제 ActivateAbility 로직은 Blueprint에서 구현한다.
 */
UCLASS()
class DAERUNE_API UDRVendingMachineAttackSpeedBuff : public UDRGameplayAbility
{
	GENERATED_BODY()

protected:
	/** 적용할 공격속도 버프 GE 클래스 (BP에서 설정) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Skill")
	TSubclassOf<UGameplayEffect> AttackSpeedBuffEffect;
};
