// Copyright DaeRune

#include "Actor/Stage2/DRS2MovingBlocker.h"

#include "Components/StaticMeshComponent.h"

ADRS2MovingBlocker::ADRS2MovingBlocker()
{
	MovingRoot = CreateDefaultSubobject<USceneComponent>(TEXT("MovingRoot"));
	MovingRoot->SetupAttachment(GetRootComponent());

	BlockerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockerMesh"));
	BlockerMesh->SetupAttachment(MovingRoot);
	BlockerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRS2MovingBlocker::ApplyPose(float Alpha)
{
	if (!MovingRoot) return;

	// Alpha 0 = 열림 포즈, 1 = 막힘 포즈
	MovingRoot->SetRelativeLocation(FMath::Lerp(OpenOffset, BlockedOffset, Alpha));
}
