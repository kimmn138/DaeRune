// Copyright DaeRune


#include "Character/DRCharacter.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "NiagaraComponent.h"
#include "AbilitySystem/Debuff/DebuffNiagaraComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UI/HUD/DRHUD.h"

ADRCharacter::ADRCharacter()
{
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 400.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->SetRelativeLocation(FVector(30.f, 0.f, 50.f));
	CameraBoom->TargetArmLength = 0.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = true;
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

int32 ADRCharacter::GetPlayerLevel_Implementation()
{
	const ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();
	check(DRPlayerState);
	return DRPlayerState->GetPlayerLevel();
}

EPlayerCharacterClass ADRCharacter::GetPlayerCharacterClass_Implementation()
{
	return CharacterClass;
}

void ADRCharacter::OnRep_Stunned()
{
	if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(AbilitySystemComponent))
	{
		const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
		FGameplayTagContainer BlockedTags;
		BlockedTags.AddTag(GameplayTags.Player_Block_InputHeld);
		BlockedTags.AddTag(GameplayTags.Player_Block_InputPressed);
		BlockedTags.AddTag(GameplayTags.Player_Block_InputReleased);
		if (bIsStunned)
		{
			DRASC->AddLooseGameplayTags(BlockedTags);
			StunDebuffComponent->Activate();
		}
		else
		{
			DRASC->RemoveLooseGameplayTags(BlockedTags);
			StunDebuffComponent->Deactivate();
		}
	}
}

void ADRCharacter::OnRep_Burned()
{
	if (bIsBurned)
	{
		BurnDebuffComponent->Activate();
	}
	else
	{
		BurnDebuffComponent->Deactivate();
	}
}

void ADRCharacter::InitializeDefaultAttributes() const
{
	UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes(this, CharacterClass, 1.0f, AbilitySystemComponent);
}

void ADRCharacter::InitAbilityActorInfo()
{
	ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();
	check(DRPlayerState);
	DRPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(DRPlayerState, this);
	Cast<UDRAbilitySystemComponent>(DRPlayerState->GetAbilitySystemComponent())->AbilityActorInfoSet();
	AbilitySystemComponent = DRPlayerState->GetAbilitySystemComponent();
	AttributeSet = DRPlayerState->GetAttributeSet();
	OnAscRegistered.Broadcast(AbilitySystemComponent);
	AbilitySystemComponent->RegisterGameplayTagEvent(FDRGameplayTags::Get().Debuff_Stun, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ADRCharacter::StunTagChanged);

	if (ADRPlayerController* DRPlayerController = Cast<ADRPlayerController>(GetController()))
	{
		if (ADRHUD* DRHUD = Cast<ADRHUD>(DRPlayerController->GetHUD()))
		{
			DRHUD->InitOverlay(DRPlayerController, DRPlayerState, AbilitySystemComponent, AttributeSet);
		}
	}
	InitializeDefaultAttributes();
}
