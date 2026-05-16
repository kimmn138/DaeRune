// Copyright DaeRune


#include "Tutorial/DRTutorialFallZone.h"
#include "Character/DRCharacter.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADRTutorialFallZone::ADRTutorialFallZone()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>("TriggerBox");
	SetRootComponent(TriggerBox);
	TriggerBox->SetBoxExtent(FVector(500.f, 500.f, 100.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ADRTutorialFallZone::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADRTutorialFallZone::OnTriggerBeginOverlap);
	}
}

void ADRTutorialFallZone::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ADRCharacter* PlayerCharacter = Cast<ADRCharacter>(OtherActor);
	if (!PlayerCharacter) return;

	PlayerCharacter->TeleportTo(RespawnTransform.GetLocation(), RespawnTransform.Rotator());

	if (UCharacterMovementComponent* MoveComp = PlayerCharacter->GetCharacterMovement())
	{
		MoveComp->Velocity = FVector::ZeroVector;
	}
}
