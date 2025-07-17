#pragma once

#include "GameplayEffectTypes.h"
#include "DRAbilityTypes.generated.h"

class UGameplayEffect;

/**
 * FDamageEffectParams
 *
 * 데미지 이펙트 파라미터 구조체 정의문서
 */
USTRUCT(BlueprintType)
struct FDamageEffectParams // 데미지 이펙트 파라미터 구조체임
{
	GENERATED_BODY()

	// 기본 생성자임
	FDamageEffectParams() {}

	// 월드 컨텍스트 객체 참조태그임
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UObject> WorldContextObject = nullptr; // 월드 컨텍스트 객체포인터임

	// 적용할 데미지 GameplayEffect 클래스 참조태그임
	UPROPERTY(BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass = nullptr; // 데미지 이펙트 클래스 타입임

	// 데미지 발생 출처 ASC 참조태그임
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UAbilitySystemComponent> SourceAbilitySystemComponent; // 소스 ASC 포인터임

	// 데미지를 받을 대상 ASC 참조태그임
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UAbilitySystemComponent> TargetAbilitySystemComponent; // 타겟 ASC 포인터임

	// 기본 데미지 수치임
	UPROPERTY(BlueprintReadWrite)
	float BaseDamage = 0.f; // 기본 데미지 플로트임

	// 능력 레벨임
	UPROPERTY(BlueprintReadWrite)
	float AbilityLevel = 1.f; // 능력 레벨 플로트임

	// 데미지 타입 태그임
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag DamageType = FGameplayTag(); // 데미지 타입 태그임

	// 디버프 발생 확률임
	UPROPERTY(BlueprintReadWrite)
	float DebuffChance = 0.f; // 디버프 확률 플로트임

	// 디버프 데미지량임
	UPROPERTY(BlueprintReadWrite)
	float DebuffDamage = 0.f; // 디버프 데미지 플로트임

	// 디버프 지속시간임
	UPROPERTY(BlueprintReadWrite)
	float DebuffDuration = 0.f; // 디버프 지속시간 플로트임

	// 디버프 빈도임
	UPROPERTY(BlueprintReadWrite)
	float DebuffFrequency = 0.f; // 디버프 빈도 플로트임

	// 사망시 적용 임펄스 세기임
	UPROPERTY(BlueprintReadWrite)
	float DeathImpulseMagnitude = 0.f; // 사망 임펄스 세기 플로트임

	// 사망 임펄스 방향 벡터임
	UPROPERTY(BlueprintReadWrite)
	FVector DeathImpulse = FVector::ZeroVector; // 사망 임펄스 벡터임

	// 넉백 힘 세기임
	UPROPERTY(BlueprintReadWrite)
	float KnockbackForceMagnitude = 0.f; // 넉백 세기 플로트임

	// 넉백 발생 확률임
	UPROPERTY(BlueprintReadWrite)
	float KnockbackChance = 0.f; // 넉백 확률 플로트임

	// 넉백 방향 벡터임
	UPROPERTY(BlueprintReadWrite)
	FVector KnockbackForce = FVector::ZeroVector; // 넉백 벡터임
};

/**
 * FDRGameplayEffectContext
 *
 * 커스텀 GameplayEffectContext 서브클래스 정의문서
 */
USTRUCT(BlueprintType)
struct FDRGameplayEffectContext : public FGameplayEffectContext // 커스텀 이펙트 컨텍스트 구조체임
{
	GENERATED_BODY()

public:
	// 디버프 성공 여부 반환함수임
	bool IsSuccessfulDebuff() const { return bIsSuccessfulDebuff; }
	// 디버프 데미지량 반환함수임
	float GetDebuffDamage() const { return DebuffDamage; }
	// 디버프 지속시간 반환함수임
	float GetDebuffDuration() const { return DebuffDuration; }
	// 디버프 빈도 반환함수임
	float GetDebuffFrequency() const { return DebuffFrequency; }
	// 데미지 타입 태그 반환함수임
	TSharedPtr<FGameplayTag> GetDamageType() const { return DamageType; }
	// 사망 임펄스 벡터 반환함수임
	FVector GetDeathImpulse() const { return DeathImpulse; }
	// 넉백 힘 벡터 반환함수임
	FVector GetKnockbackForce() const { return KnockbackForce; }

	// 디버프 상태 설정함수임
	void SetIsSuccessfulDebuff(bool bInIsDebuff) { bIsSuccessfulDebuff = bInIsDebuff; }
	// 디버프 데미지량 설정함수임
	void SetDebuffDamage(float InDamage) { DebuffDamage = InDamage; }
	// 디버프 지속시간 설정함수임
	void SetDebuffDuration(float InDuration) { DebuffDuration = InDuration; }
	// 디버프 빈도 설정함수임
	void SetDebuffFrequency(float InFrequency) { DebuffFrequency = InFrequency; }
	// 데미지 타입 태그 설정함수임
	void SetDamageType(TSharedPtr<FGameplayTag> InDamageType) { DamageType = InDamageType; }
	// 사망 임펄스 설정함수임
	void SetDeathImpulse(const FVector& InImpulse) { DeathImpulse = InImpulse; }
	// 넉백 힘 설정함수임
	void SetKnockbackForce(const FVector& InForce) { KnockbackForce = InForce; }

	/** 스크립트 직렬화용 구조체 반환함수임 */
	virtual UScriptStruct* GetScriptStruct() const
	{
		return FGameplayEffectContext::StaticStruct(); // 베이스 컨텍스트 구조체 반환임
	}

	/** 컨텍스트 복제용 함수임 */
	virtual FGameplayEffectContext* Duplicate() const
	{
		FGameplayEffectContext* NewContext = new FGameplayEffectContext(); // 새 컨텍스트 객체 생성임
		*NewContext = *this; // 얕은 복사 실행임
		if (GetHitResult())
		{
			// 히트 결과 깊은 복사 처리임
			NewContext->AddHitResult(*GetHitResult(), true);
		}
		return NewContext; // 복제 컨텍스트 반환임
	}

	/** 네트워크 직렬화 함수 정의임 */
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	// 네트워크 전송 디버프 성공 여부 플래그임
	UPROPERTY()
	bool bIsSuccessfulDebuff = false;

	// 네트워크 전송 디버프 데미지량 플로트임
	UPROPERTY()
	float DebuffDamage = 0.f;

	// 네트워크 전송 디버프 지속시간 플로트임
	UPROPERTY()
	float DebuffDuration = 0.f;

	// 네트워크 전송 디버프 빈도 플로트임
	UPROPERTY()
	float DebuffFrequency = 0.f;

	// 데미지 타입 태그 공유포인터임
	TSharedPtr<FGameplayTag> DamageType;

	// 사망 임펄스 벡터임
	UPROPERTY()
	FVector DeathImpulse = FVector::ZeroVector;

	// 넉백 힘 벡터임
	UPROPERTY()
	FVector KnockbackForce = FVector::ZeroVector;
};

// TStructOpsTypeTraits 특수화 정의문서임
template<>
struct TStructOpsTypeTraits<FDRGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FDRGameplayEffectContext>
{
	enum
	{
		WithNetSerializer = true, // 네트워크 직렬화 사용 플래그임
		WithCopy = true // 복사 기능 사용 플래그임
	};
};
