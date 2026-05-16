// Copyright DaeRune


#include "AbilitySystem/Abilities/DRVendingMachineAttackSpeedBuff.h"
#include "AbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffect.h"
#include "Character/DRCharacter.h"
#include "DRGameplayTags.h"

void UDRVendingMachineAttackSpeedBuff::NotifySkillActivated()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority()) return;

	int32 Stacks = 0;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayTagContainer Filter;
		Filter.AddTag(FDRGameplayTags::Get().Buff_VendingMachine_AttackSpeed);

		TArray<FActiveGameplayEffectHandle> Active = ASC->GetActiveEffectsWithAllTags(Filter);
		for (const FActiveGameplayEffectHandle& H : Active)
		{
			if (const FActiveGameplayEffect* GE = ASC->GetActiveGameplayEffect(H))
			{
				Stacks = GE->Spec.GetStackCount();
				break;
			}
		}
	}

	// GE Apply 직후 호출되는 시점이므로 현재 조회되는 Stacks가 곧 사용 후의 스택 수.
	// 만약 BP에서 Apply 이전에 호출한다면 +1 보정 필요.
	const int32 ClampedStacks = FMath::Clamp(Stacks, 1, 255);

	if (ADRCharacter* DRChar = Cast<ADRCharacter>(Avatar))
	{
		DRChar->MulticastPlayVendingSkillUse(static_cast<uint8>(ClampedStacks));
	}
}
