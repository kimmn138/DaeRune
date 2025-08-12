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

    // Water Cost 설정 (블루프린트에서 설정 가능)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost")
    float WaterCost = 0.f;

    // Cost 체크 오버라이드
    virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;

    // Cost 적용 오버라이드  
    virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
};
