// Copyright DaeRune


#include "Player/DRPlayerState.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Net/UnrealNetwork.h"

ADRPlayerState::ADRPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UDRAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UDRAttributeSet>("AttributeSet");
	
	NetUpdateFrequency = 100.f;
}

UAbilitySystemComponent* ADRPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
