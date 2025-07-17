// Copyright DaeRune


#include "AbilitySystem/DRAbilitySystemGlobals.h"
#include "DRAbilityTypes.h"

/**
 * AllocGameplayEffectContext 구현: FDRGameplayEffectContext 인스턴스 생성 기능
 */
FGameplayEffectContext* UDRAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	// 커스텀 GameplayEffectContext 객체 생성 단계
	return new FDRGameplayEffectContext();
}
