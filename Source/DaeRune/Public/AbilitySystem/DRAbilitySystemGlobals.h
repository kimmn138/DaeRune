// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemGlobals.h"
#include "DRAbilitySystemGlobals.generated.h"

/**
 * UDRAbilitySystemGlobals 클래스: 커스텀 GameplayEffectContext 생성 기능을 제공하는 전역 설정 클래스
 */
UCLASS()
class DAERUNE_API UDRAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()
	
	/**
	 * AllocGameplayEffectContext 오버라이드: DaeRune 전용 FDRGameplayEffectContext 인스턴스 할당 기능
	 */
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;
};
