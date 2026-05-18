// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DRGameplayAbility.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag StartupInputTag;

    // Water Cost ���� (��������Ʈ���� ���� ����)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost")
    float WaterCost = 0.f;

    // Cost üũ �������̵�
    virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;

    // Cost ���� �������̵�
    virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

    // 스킬 차단 UI 트리거를 위해 AbilityTags 를 ActivationOwnedTags 에 자동 머지
    virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
};
