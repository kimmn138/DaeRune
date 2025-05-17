// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/DRPlayerController.h"
#include "EnhancedInputSubsystems.h"

ADRPlayerController::ADRPlayerController()
{
	bReplicates = true;
}

void ADRPlayerController::BeginPlay()
{
	Super::BeginPlay();
	check(DRContext);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	check(Subsystem);
	Subsystem->AddMappingContext(DRContext, 0);

	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;

	FInputModeGameAndUI InputModeData;
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputModeData.SetHideCursorDuringCapture(false);
	SetInputMode(InputModeData);
}
