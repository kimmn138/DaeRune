// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "DRAbilityTypes.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "Interaction/CombatInterface.h"
#include "DRDamageGameplayAbility.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRDamageGameplayAbility : public UDRGameplayAbility
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void CauseDamage(AActor* TargetActor);

	UFUNCTION(BlueprintPure)
	FDamageEffectParams MakeDamageEffectParamsFromClassDefaults(AActor* TargetActor = nullptr) const;

	UFUNCTION(BlueprintPure)
	float GetDamageAtLevel() const;

	// 업그레이드 칩(SkillDamage)이 반영된 피해량.
	// 피해량 조회는 반드시 이 함수 하나만 쓴다 — 호출부가 흩어져 누락되는 것을 막기 위함. (Plan2.md 8.5)
	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetUpgradedDamage() const;

	/**
	 * ★Plan5(물대포칩)★ 런타임 조건에 따른 피해 배율 훅. 기본 1.0(변화 없음).
	 *
	 * 칩 장착 여부처럼 "고정된" 보정은 EDRUpgradeStat::SkillDamage 로 표현한다.
	 * 이 훅은 ★채널링 스택처럼 매 순간 변하는★ 보정만을 위한 것이다.
	 *
	 * GetUpgradedDamage() 가 마지막에 이 값을 곱하므로, 파생 클래스는 이것만 오버라이드하면
	 * 모든 피해 경로(MakeDamageEffectParamsFromClassDefaults / CauseDamage / GetDamageAtLevel)에
	 * 자동으로 반영된다.
	 *
	 * (선례: UDRGameplayAbility::GetBaseCooldownDuration — 같은 목적의 런타임 훅이다)
	 */
	virtual float GetRuntimeDamageMultiplier() const { return 1.f; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	FGameplayTag DamageType;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	FScalableFloat Damage;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float DebuffChance = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float DebuffDamage = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float DebuffFrequency = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float DebuffDuration = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float DeathImpulseMagnitude = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float KnockbackForceMagnitude = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float KnockbackChance = 0.f;

	UFUNCTION(BlueprintPure)
	FTaggedMontage GetRandomTaggedMontageFromArray(const TArray<FTaggedMontage>& TaggedMontages) const;
};
