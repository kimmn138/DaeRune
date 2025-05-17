// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/DRCharacter.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/DRPlayerState.h"

ADRCharacter::ADRCharacter()
{
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 400.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
}

void ADRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Init ability actor info for the Server
	InitAbilityActorInfo();
}

void ADRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Init ability actor info for the Client
	InitAbilityActorInfo();
}

void ADRCharacter::InitAbilityActorInfo()
{
	ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();
	check(DRPlayerState);
	DRPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(DRPlayerState, this);
	AbilitySystemComponent = DRPlayerState->GetAbilitySystemComponent();
	AttributeSet = DRPlayerState->GetAttributeSet();
}
