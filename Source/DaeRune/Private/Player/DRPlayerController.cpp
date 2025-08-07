// Copyright DaeRune


#include "Player/DRPlayerController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DRGameplayTags.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "Input/DRInputComponent.h"
#include "GameFramework/Character.h"
#include "UI/Widget/DamageTextComponent.h"

ADRPlayerController::ADRPlayerController()
{
	bReplicates = true;
}

void ADRPlayerController::OnCorruptedStateChanged(bool bIsStateChanged)
{
	bIsCorrupted = bIsStateChanged;

	// 음성 채팅 설정
	//SetVoiceChatEnabled(!bIsCorrupted);

	// 팀 구분 시각 효과 설정
	//SetTeamVisualsEnabled(!bIsCorrupted);

	if (bIsCorrupted)
	{
		// 부패 상태 진입 시 추가 처리
		UE_LOG(LogTemp, Warning, TEXT("Player entered corrupted state"));
	}
	else
	{
		// 부패 상태 해제 시 추가 처리
		UE_LOG(LogTemp, Warning, TEXT("Player exited corrupted state"));
	}
}

void ADRPlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter)
{
	if (IsValid(TargetCharacter) && DamageTextComponentClass && IsLocalController())
	{
		UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);
		DamageText->RegisterComponent();
		DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		DamageText->SetDamageText(DamageAmount);
	}
}

void ADRPlayerController::BeginPlay()
{
	Super::BeginPlay();
	check(DRContext);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->AddMappingContext(DRContext, 0);
	}

	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

void ADRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UDRInputComponent* DRInputComponent = CastChecked<UDRInputComponent>(InputComponent);
	DRInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Move);
	DRInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Look);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ADRPlayerController::StartJump);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ADRPlayerController::StopJump);
	DRInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}

void ADRPlayerController::Move(const FInputActionValue& InputActionValue)
{
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed))
	{
		return; 
	}
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);
	}
}

void ADRPlayerController::Look(const FInputActionValue& InputActionValue)
{
	const FVector2D Axis = InputActionValue.Get<FVector2D>();
	AddYawInput(Axis.X);
	AddPitchInput(Axis.Y);
}

void ADRPlayerController::StartJump(const FInputActionValue& InputActionValue)
{
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn<APawn>()))
	{
		ControlledCharacter->Jump();
	}
}

void ADRPlayerController::StopJump(const FInputActionValue& InputActionValue)
{
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn<APawn>()))
	{
		ControlledCharacter->StopJumping();
	}
}

void ADRPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed))
	{
		return;
	}
	if (GetASC()) GetASC()->AbilityInputTagPressed(InputTag);
}

void ADRPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputReleased))
	{
		return;
	}
	if (GetASC() == nullptr) return;
	GetASC()->AbilityInputTagReleased(InputTag);
}

void ADRPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputHeld))
	{
		return;
	}
	if (GetASC() == nullptr) return;
	GetASC()->AbilityInputTagHeld(InputTag);
}

UDRAbilitySystemComponent* ADRPlayerController::GetASC()
{
	if (DRAbilitySystemComponent == nullptr)
	{
		DRAbilitySystemComponent = Cast<UDRAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
	}
	return DRAbilitySystemComponent;
}
