// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "DRAbilityTypes.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "Interaction/CombatInterface.h"
#include "DRDamageGameplayAbility.generated.h"

/**
 * 데미지 처리용 커스텀 GameplayAbility 클래스 선언
 */
UCLASS()
class DAERUNE_API UDRDamageGameplayAbility : public UDRGameplayAbility
{
	GENERATED_BODY()
	
public:
	// 데미지 적용 함수 선언
	UFUNCTION(BlueprintCallable)
	void CauseDamage(AActor* TargetActor);

	// 클래스 기본 설정 기반 데미지 이펙트 파라미터 생성 함수 선언
	UFUNCTION(BlueprintPure)
	FDamageEffectParams MakeDamageEffectParamsFromClassDefaults(AActor* TargetActor = nullptr) const;

	// 레벨 기반 데미지 값 반환 함수 선언
	UFUNCTION(BlueprintPure)
	float GetDamageAtLevel() const;
	
protected:
	// 데미지 이펙트 클래스 변수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 데미지 타입 태그 변수
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	FGameplayTag DamageType;

	// 스케일 가능 데미지 변수
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	FScalableFloat Damage;

	// 디버프 적용 확률 변수
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float DebuffChance = 20.f;

	// 디버프 데미지 값 변수
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float DebuffDamage = 5.f;

	// 디버프 주기 변수
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float DebuffFrequency = 1.f;

	// 디버프 지속시간 변수
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float DebuffDuration = 5.f;

	// 죽음 임펄스 세기 변수
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float DeathImpulseMagnitude = 1000.f;

	// 넉백 힘 세기 변수
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float KnockbackForceMagnitude = 1000.f;

	// 넉백 적용 확률 변수
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float KnockbackChance = 0.f;

	// 태그된 몽타주 배열에서 랜덤 몽타주 반환 함수 선언
	UFUNCTION(BlueprintPure)
	FTaggedMontage GetRandomTaggedMontageFromArray(const TArray<FTaggedMontage>& TaggedMontages) const;
};
