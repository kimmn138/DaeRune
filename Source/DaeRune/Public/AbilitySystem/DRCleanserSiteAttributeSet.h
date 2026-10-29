// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAttributeAccessors.h"
#include "DRCleanserSiteAttributeSet.generated.h"

// 클렌저 사이트 파괴 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCleanserSiteHealthZeroSignature);

/**
 * 클렌저 사이트 전용 AttributeSet
 * Phase3에서 방어 대상으로 사용될 때 체력 관리
 */
UCLASS()
class DAERUNE_API UDRCleanserSiteAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
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

	// ========== Delegates ==========

	// 체력 0 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "CleanserSite")
	FOnCleanserSiteHealthZeroSignature OnHealthZeroDelegate;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff")
	float EliteDebuffModifier = 2.0f;

private:
	// 데미지 처리 함수
	void HandleIncomingDamage(const FGameplayEffectModCallbackData& Data);
};
