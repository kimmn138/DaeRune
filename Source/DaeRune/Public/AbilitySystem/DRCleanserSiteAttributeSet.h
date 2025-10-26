// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "DRCleanserSiteAttributeSet.generated.h"

// AttributeSet 매크로 정의
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 클렌저 사이트 전용 AttributeSet
 * Phase3에서 방어 대상으로 사용될 때 체력 관리
 */
UCLASS()
class DAERUNE_API UDRCleanserSiteAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	UDRCleanserSiteAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ========== Attributes ==========

	// 현재 체력
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Vital Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UDRCleanserSiteAttributeSet, Health);

	// 최대 체력
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Vital Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UDRCleanserSiteAttributeSet, MaxHealth);

	// ========== Replication Callbacks ==========

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth) const;

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const;

	// ========== Meta Attributes (임시 데미지 계산용) ==========

	UPROPERTY(BlueprintReadOnly, Category = "Meta Attributes")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UDRCleanserSiteAttributeSet, IncomingDamage);

	// ========== Attribute 변경 처리 ==========

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
