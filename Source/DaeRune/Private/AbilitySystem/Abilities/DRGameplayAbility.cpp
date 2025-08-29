// Copyright DaeRune


#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"

bool UDRGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
    // 기본 Cost 체크
    if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
    {
        return false;
    }

    // Water Cost가 없으면 통과
    if (WaterCost <= 0.f)
    {
        return true;
    }

    // AttributeSet 가져오기
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC)
    {
        return false;
    }

    const UDRAttributeSet* AttributeSet = Cast<UDRAttributeSet>(
        ASC->GetAttributeSet(UDRAttributeSet::StaticClass()));
    if (!AttributeSet)
    {
        return false;
    }

    float CurrentWater = AttributeSet->GetWater();
    float CurrentHealth = AttributeSet->GetHealth();

    // Water가 충분한 경우
    if (CurrentWater >= WaterCost)
    {
        return true;
    }

    // Water가 부족한 경우, Health로 보충 가능한지 확인
    float WaterShortage = WaterCost - CurrentWater;
    float RequiredHealth = FMath::FloorToFloat(WaterShortage * 0.5f);

    // Health가 최소 1 이상 남을 수 있는지 확인
    return CurrentHealth > RequiredHealth;
}

void UDRGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    // 기본 Cost 적용
    Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

    if (WaterCost <= 0.f || !HasAuthority(&ActivationInfo))
    {
        return;
    }

    // Cost GE가 설정되어 있으면 SetByCaller로 WaterCost 전달
    if (GetCostGameplayEffect())
    {
        UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
        if (ASC)
        {
            FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
                GetCostGameplayEffect()->GetClass(), GetAbilityLevel());

            if (SpecHandle.IsValid())
            {
                const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
                SpecHandle.Data.Get()->SetSetByCallerMagnitude(GameplayTags.Cost_Water, WaterCost);

                ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            }
        }
    }
}
