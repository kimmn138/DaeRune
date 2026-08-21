// Copyright DaeRune

#include "Actor/Stage2/DRS2Barrier.h"

#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"

ADRS2Barrier::ADRS2Barrier()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	BlockBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BlockBox"));
	SetRootComponent(BlockBox);

	// Pawn 만 Block. 열차/투사체는 통과시켜 주행과 사격을 방해하지 않는다.
	BlockBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BlockBox->SetCollisionObjectType(ECC_WorldStatic);
	BlockBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	BlockBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	BlockBox->SetBoxExtent(FVector(100.f, 500.f, 300.f));
}

void ADRS2Barrier::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2Barrier, bEnabled);
}

void ADRS2Barrier::BeginPlay()
{
	Super::BeginPlay();

	// 기본 비활성
	BlockBox->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	OnBarrierEnabledChanged(bEnabled);
}

void ADRS2Barrier::SetBarrierEnabled(bool bNewEnabled)
{
	if (!HasAuthority() || bEnabled == bNewEnabled) return;

	bEnabled = bNewEnabled;
	OnRep_bEnabled();
}

void ADRS2Barrier::OnRep_bEnabled()
{
	BlockBox->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	OnBarrierEnabledChanged(bEnabled);
}
