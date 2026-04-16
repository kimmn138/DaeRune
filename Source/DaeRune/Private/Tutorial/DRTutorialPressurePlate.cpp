// Copyright DaeRune


#include "Tutorial/DRTutorialPressurePlate.h"
#include "Tutorial/DRTutorialManager.h"
#include "Character/DRCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

ADRTutorialPressurePlate::ADRTutorialPressurePlate()
{
	PrimaryActorTick.bCanEverTick = false;

	PlateMesh = CreateDefaultSubobject<UStaticMeshComponent>("PlateMesh");
	SetRootComponent(PlateMesh);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>("TriggerBox");
	TriggerBox->SetupAttachment(PlateMesh);
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 50.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
}

void ADRTutorialPressurePlate::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADRTutorialPressurePlate::OnTriggerBeginOverlap);
	}
}

void ADRTutorialPressurePlate::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bActivated) return;

	ADRCharacter* PlayerCharacter = Cast<ADRCharacter>(OtherActor);
	if (!PlayerCharacter) return;

	if (TutorialManager)
	{
		TutorialManager->OnPressurePlateActivated(SectionIndex);
	}
}
