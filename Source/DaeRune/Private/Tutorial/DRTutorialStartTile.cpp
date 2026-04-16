// Copyright DaeRune


#include "Tutorial/DRTutorialStartTile.h"
#include "Tutorial/DRTutorialManager.h"
#include "Character/DRCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

ADRTutorialStartTile::ADRTutorialStartTile()
{
	PrimaryActorTick.bCanEverTick = false;

	TileMesh = CreateDefaultSubobject<UStaticMeshComponent>("TileMesh");
	SetRootComponent(TileMesh);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>("TriggerBox");
	TriggerBox->SetupAttachment(TileMesh);
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 50.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
}

void ADRTutorialStartTile::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADRTutorialStartTile::OnTriggerBeginOverlap);
	}
}

void ADRTutorialStartTile::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bActivated) return;

	if (!Cast<ADRCharacter>(OtherActor)) return;

	bActivated = true;

	if (TutorialManager)
	{
		TutorialManager->OnStartTileActivated();
	}
}
