// Copyright DaeRune


#include "Character/DREnemy.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAttributeSet.h"

ADREnemy::ADREnemy()
{
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	AbilitySystemComponent = CreateDefaultSubobject<UDRAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UDRAttributeSet>("AttributeSet");
}

int32 ADREnemy::GetPlayerLevel()
{
	return Level;
}

void ADREnemy::BeginPlay()
{
	Super::BeginPlay();

	InitAbilityActorInfo();
}

void ADREnemy::InitAbilityActorInfo()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	Cast<UDRAbilitySystemComponent>(AbilitySystemComponent)->AbilityActorInfoSet();
}
