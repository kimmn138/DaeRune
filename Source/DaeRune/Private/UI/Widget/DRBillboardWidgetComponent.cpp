// Copyright DaeRune


#include "UI/Widget/DRBillboardWidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

UDRBillboardWidgetComponent::UDRBillboardWidgetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UDRBillboardWidgetComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsVisible())
	{
		return;
	}

	APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
	if (!LocalPC || !LocalPC->IsLocalController())
	{
		return;
	}

	APlayerCameraManager* CameraManager = LocalPC->PlayerCameraManager;
	if (!CameraManager)
	{
		return;
	}

	FVector CameraLocation = CameraManager->GetCameraLocation();
	FVector WidgetLocation = GetComponentLocation();

	FVector Direction = CameraLocation - WidgetLocation;
	Direction.Z = 0.0f;

	if (!Direction.IsNearlyZero())
	{
		FRotator LookAtRotation = Direction.Rotation();
		SetWorldRotation(LookAtRotation);
	}
}
