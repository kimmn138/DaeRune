// Copyright DaeRune

#include "Actor/Stage2/DRS2Barrier.h"

#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"

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

	// ★런타임 코드가 바꾸는 것은 CollisionEnabled 뿐이고, 채널 응답은 BP 에 직렬화된 값을 그대로 쓴다.
	//   BP 에서 콜리전 프리셋을 건드리면 배리어를 켜도 플레이어가 그냥 통과한다.
	if (bEnabled && BlockBox->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2Barrier] %s: BlockBox 의 Pawn 응답이 Block 이 아니라 플레이어를 막지 못합니다. ")
			TEXT("BP_S2Barrier 의 콜리전 프리셋을 확인하세요."), *GetName());
	}

	OnBarrierEnabledChanged(bEnabled);
}
