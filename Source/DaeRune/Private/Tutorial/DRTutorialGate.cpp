// Copyright DaeRune


#include "Tutorial/DRTutorialGate.h"
#include "Components/StaticMeshComponent.h"

ADRTutorialGate::ADRTutorialGate()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootScene = CreateDefaultSubobject<USceneComponent>("RootScene");
	SetRootComponent(RootScene);

	GateMeshLeft = CreateDefaultSubobject<UStaticMeshComponent>("GateMeshLeft");
	GateMeshLeft->SetupAttachment(RootScene);
	GateMeshLeft->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GateMeshLeft->SetCollisionResponseToAllChannels(ECR_Block);

	GateMeshRight = CreateDefaultSubobject<UStaticMeshComponent>("GateMeshRight");
	GateMeshRight->SetupAttachment(RootScene);
	GateMeshRight->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GateMeshRight->SetCollisionResponseToAllChannels(ECR_Block);
}

void ADRTutorialGate::BeginPlay()
{
	Super::BeginPlay();

	LeftClosedLocation = GateMeshLeft->GetRelativeLocation();
	RightClosedLocation = GateMeshRight->GetRelativeLocation();

	// +X 방향으로 열리는 왼쪽 문, -X 방향으로 열리는 오른쪽 문
	LeftOpenLocation = LeftClosedLocation + FVector(OpenDistance, 0.f, 0.f);
	RightOpenLocation = RightClosedLocation + FVector(-OpenDistance, 0.f, 0.f);
}

void ADRTutorialGate::OpenGate()
{
	if (bIsOpen || bIsOpening) return;

	bIsOpening = true;
	SetActorTickEnabled(true);
}

void ADRTutorialGate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsOpening || bIsOpen) return;

	FVector LeftCurrent = GateMeshLeft->GetRelativeLocation();
	FVector RightCurrent = GateMeshRight->GetRelativeLocation();

	FVector LeftNew = FMath::VInterpConstantTo(LeftCurrent, LeftOpenLocation, DeltaTime, OpenSpeed);
	FVector RightNew = FMath::VInterpConstantTo(RightCurrent, RightOpenLocation, DeltaTime, OpenSpeed);

	GateMeshLeft->SetRelativeLocation(LeftNew);
	GateMeshRight->SetRelativeLocation(RightNew);

	bool bLeftDone = FVector::Dist(LeftNew, LeftOpenLocation) < 1.f;
	bool bRightDone = FVector::Dist(RightNew, RightOpenLocation) < 1.f;

	if (bLeftDone && bRightDone)
	{
		GateMeshLeft->SetRelativeLocation(LeftOpenLocation);
		GateMeshRight->SetRelativeLocation(RightOpenLocation);
		bIsOpen = true;
		bIsOpening = false;
		SetActorTickEnabled(false);

		GateMeshLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GateMeshRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}
