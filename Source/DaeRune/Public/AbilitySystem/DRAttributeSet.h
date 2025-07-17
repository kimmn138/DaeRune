// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "DRAttributeSet.generated.h"

// 속성 접근자 매크로 정의
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

// 효과 속성 저장용 구조체
USTRUCT()
struct FEffectProperties
{
	GENERATED_BODY()

	// 기본 생성자
	FEffectProperties() {}

	FGameplayEffectContextHandle EffectContextHandle; // 이펙트 컨텍스트 핸들

	UPROPERTY()
	UAbilitySystemComponent* SourceASC = nullptr; // 소스 ASC 포인터

	UPROPERTY()
	AActor* SourceAvatarActor = nullptr; // 소스 아바타 액터 포인터

	UPROPERTY()
	AController* SourceController = nullptr; // 소스 컨트롤러 포인터

	UPROPERTY()
	ACharacter* SourceCharacter = nullptr; // 소스 캐릭터 포인터

	UPROPERTY()
	UAbilitySystemComponent* TargetASC = nullptr; // 타겟 ASC 포인터

	UPROPERTY()
	AActor* TargetAvatarActor = nullptr; // 타겟 아바타 액터 포인터

	UPROPERTY()
	AController* TargetController = nullptr; // 타겟 컨트롤러 포인터

	UPROPERTY()
	ACharacter* TargetCharacter = nullptr; // 타겟 캐릭터 포인터
};

// 템플릿 함수 포인터 타입 정의
// typedef is specific to the FGameplayAttribute() signature, but TStaticFunPtr is generic to any signature chosen
//typedef TBaseStaticDelegateInstance<FGameplayAttribute(), FDefaultDelegateUserPolicy>::FFuncPtr FAttributeFuncPtr;
template<class T>
using TStaticFuncPtr = typename TBaseStaticDelegateInstance<T, FDefaultDelegateUserPolicy>::FFuncPtr;

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	UDRAttributeSet();
	// 복제 프로퍼티 등록 함수
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 속성 변경 전 처리 함수
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	// 이펙트 실행 후 처리 함수
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	// 속성 변경 후 처리 함수
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	// 태그-속성 매핑 테이블
	TMap<FGameplayTag, TStaticFuncPtr<FGameplayAttribute()>> TagsToAttributes;

	/*
	 * Primary Attributes
	 */

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Vital Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UDRAttributeSet, MaxHealth);

	/*
	 * Vital Attributes
	 */

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Vital Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UDRAttributeSet, Health);

	/*
	 * Meta Attributes
	 */

	UPROPERTY(BlueprintReadOnly, Category = "Meta Attributes")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UDRAttributeSet, IncomingDamage);

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth) const; // Health 복제 알림 함수

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const; // MaxHealth 복제 알림 함수

private:
	// 들어오는 피해 처리 함수
	void HandleIncomingDamage(const FEffectProperties& Props);
	// 디버프 생성 함수
	void Debuff(const FEffectProperties& Props);
	// 이펙트 속성 설정 함수
	void SetEffectProperties(const FGameplayEffectModCallbackData& Data, FEffectProperties& Props) const;
	// 플로팅 텍스트 표시 함수
	void ShowFloatingText(const FEffectProperties& Props, float Damage) const;
	// 최대 체력 보정 플래그
	bool bTopOffHealth = false;
};
