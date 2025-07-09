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

void ADRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRPlayerState, Level);
}

UAbilitySystemComponent* ADRPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADRPlayerState::OnRep_Level(int32 OldLevel)
{
}
