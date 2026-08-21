// Copyright DaeRune

#include "Actor/Stage2/DRS2RoomTrigger.h"

#include "Components/BoxComponent.h"
#include "Character/DRCharacter.h"
#include "Game/DRGameStateBase.h"
#include "TimerManager.h"
#include "DaeRune/DRLogChannels.h"

ADRS2RoomTrigger::ADRS2RoomTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	// Pawn 오버랩만 감지 (판정은 서버에서만 수행)
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldStatic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetBoxExtent(FVector(500.f, 500.f, 300.f));
	TriggerBox->SetGenerateOverlapEvents(true);

	bReplicates = false; // 서버 전용 판정 액터 (연출 없음)
}

void ADRS2RoomTrigger::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority()) return;

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADRS2RoomTrigger::HandleBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ADRS2RoomTrigger::HandleEndOverlap);
}

void ADRS2RoomTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReevaluateTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void ADRS2RoomTrigger::Arm()
{
	if (!HasAuthority() || bArmed) return;

	bArmed = true;
	bFired = false;
	LastBroadcastInside = -1;
	LastBroadcastTotal = -1;

	// 이미 박스 안에 있는 플레이어를 초기 집합에 반영 (Arm 시점에 서 있는 경우)
	PlayersInside.Reset();
	TArray<AActor*> Overlapping;
	TriggerBox->GetOverlappingActors(Overlapping, ADRCharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		if (ADRCharacter* Character = Cast<ADRCharacter>(Actor))
		{
			PlayersInside.Add(Character);
		}
	}

	// 오버랩 이벤트가 발생하지 않는 변화(방 밖에서의 사망 등)를 잡기 위한 주기 재평가
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ReevaluateTimer, this, &ADRS2RoomTrigger::Evaluate, ReevaluateInterval, true);
	}

	UE_LOG(LogDR, Verbose, TEXT("[S2Trigger] Arm: %s"), *RoomID.ToString());

	Evaluate();
}

void ADRS2RoomTrigger::Disarm()
{
	if (!HasAuthority()) return;

	bArmed = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReevaluateTimer);
	}

	UE_LOG(LogDR, Verbose, TEXT("[S2Trigger] Disarm: %s"), *RoomID.ToString());
}

void ADRS2RoomTrigger::ReArm()
{
	if (!HasAuthority()) return;

	// 발화 래치만 풀고 감시는 유지한다.
	bFired = false;
	LastBroadcastInside = -1;

	UE_LOG(LogDR, Verbose, TEXT("[S2Trigger] ReArm: %s"), *RoomID.ToString());
}

bool ADRS2RoomTrigger::AreAllAlivePlayersInside() const
{
	const ADRGameStateBase* DRGameState = GetWorld() ? GetWorld()->GetGameState<ADRGameStateBase>() : nullptr;
	if (!DRGameState) return false;

	const TArray<ADRCharacter*> AlivePlayers = DRGameState->GetAlivePlayers();
	if (AlivePlayers.Num() == 0) return false;

	for (ADRCharacter* Character : AlivePlayers)
	{
		if (!PlayersInside.Contains(Character))
		{
			return false;
		}
	}

	return true;
}

bool ADRS2RoomTrigger::IsActorInside(const AActor* TargetActor) const
{
	if (!IsValid(TargetActor) || !TriggerBox) return false;

	return TriggerBox->IsOverlappingActor(TargetActor);
}

void ADRS2RoomTrigger::HandleBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!HasAuthority()) return;

	if (ADRCharacter* Character = Cast<ADRCharacter>(OtherActor))
	{
		PlayersInside.Add(Character);
		Evaluate();
	}
}

void ADRS2RoomTrigger::HandleEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (!HasAuthority()) return;

	if (ADRCharacter* Character = Cast<ADRCharacter>(OtherActor))
	{
		PlayersInside.Remove(Character);
		Evaluate();
	}
}

void ADRS2RoomTrigger::PruneInvalidPlayers()
{
	for (auto It = PlayersInside.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void ADRS2RoomTrigger::Evaluate()
{
	if (!HasAuthority() || !bArmed) return;

	PruneInvalidPlayers();

	const ADRGameStateBase* DRGameState = GetWorld() ? GetWorld()->GetGameState<ADRGameStateBase>() : nullptr;
	if (!DRGameState) return;

	const TArray<ADRCharacter*> AlivePlayers = DRGameState->GetAlivePlayers();

	// 전멸 상태는 GameMode가 처리한다. 여기서 0/0을 "전원 입장"으로 오판하지 않도록 방어.
	if (AlivePlayers.Num() == 0) return;

	int32 InsideAliveCount = 0;
	for (ADRCharacter* Character : AlivePlayers)
	{
		if (PlayersInside.Contains(Character))
		{
			++InsideAliveCount;
		}
	}

	// 진행도 UI는 값이 바뀔 때만 브로드캐스트 (폴링이라 매 틱 호출된다)
	if (InsideAliveCount != LastBroadcastInside || AlivePlayers.Num() != LastBroadcastTotal)
	{
		LastBroadcastInside = InsideAliveCount;
		LastBroadcastTotal = AlivePlayers.Num();
		OnCountChanged.Broadcast(InsideAliveCount, AlivePlayers.Num());
	}

	if (!bFired && InsideAliveCount == AlivePlayers.Num())
	{
		bFired = true;
		UE_LOG(LogDR, Log, TEXT("[S2Trigger] 전원 입장: %s (%d명)"), *RoomID.ToString(), InsideAliveCount);
		OnAllInside.Broadcast();
	}
}
