// Copyright DaeRune


#include "Character/DRCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "UI/HUD/DRHUD.h"

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
	AddCharacterAbilities();
}

void ADRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Init ability actor info for the Client
	InitAbilityActorInfo();
}

int32 ADRCharacter::GetPlayerLevel()
{
	const ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();
	check(DRPlayerState);
	return DRPlayerState->GetPlayerLevel();
}

void ADRCharacter::InitAbilityActorInfo()
{
	ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();
	check(DRPlayerState);
	DRPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(DRPlayerState, this);
	Cast<UDRAbilitySystemComponent>(DRPlayerState->GetAbilitySystemComponent())->AbilityActorInfoSet();
	AbilitySystemComponent = DRPlayerState->GetAbilitySystemComponent();
	AttributeSet = DRPlayerState->GetAttributeSet();

	if (ADRPlayerController* DRPlayerController = Cast<ADRPlayerController>(GetController()))
	{
		if (ADRHUD* DRHUD = Cast<ADRHUD>(DRPlayerController->GetHUD()))
		{
			DRHUD->InitOverlay(DRPlayerController, DRPlayerState, AbilitySystemComponent, AttributeSet);
		}
	}
	InitializeDefaultAttributes();
}
