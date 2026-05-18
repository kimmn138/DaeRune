// Copyright DaeRune


#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"

bool UDRGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
    // �⺻ Cost üũ
    if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
    {
        return false;
    }

    // Water Cost�� ������ ���
    if (WaterCost <= 0.f)
    {
        return true;
    }

    // AttributeSet ��������
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

    // Water�� ����� ���
    if (CurrentWater >= WaterCost)
    {
        return true;
    }

    // Water�� ������ ���, Health�� ���� �������� Ȯ��
    float WaterShortage = WaterCost - CurrentWater;
    float RequiredHealth = FMath::FloorToFloat(WaterShortage * 0.5f);

    // Health�� �ּ� 1 �̻� ���� �� �ִ��� Ȯ��
    return CurrentHealth > RequiredHealth;
}

void UDRGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    // �⺻ Cost ����
    Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

    if (WaterCost <= 0.f || !HasAuthority(&ActivationInfo))
    {
        return;
    }

    // Cost GE�� �����Ǿ� ������ SetByCaller�� WaterCost ����
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

void UDRGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    Super::OnGiveAbility(ActorInfo, Spec);

    // ActivationOwnedTags 가 AbilityTags 를 전부 포함하도록 보장.
    // 이렇게 해야 owner ASC 의 태그 카운트 이벤트가 곧 BlockAbilitiesWithTag 변화의 트리거가 되어
    // 스킬아이콘 차단 UI 가 즉시 갱신된다.
    for (const FGameplayTag& Tag : AbilityTags)
    {
        if (!ActivationOwnedTags.HasTagExact(Tag))
        {
            ActivationOwnedTags.AddTag(Tag);
        }
    }
}
