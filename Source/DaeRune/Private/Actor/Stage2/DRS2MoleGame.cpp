// Copyright DaeRune

#include "Actor/Stage2/DRS2MoleGame.h"

#include "Actor/Stage2/DRS2Mole.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "DaeRune/DRLogChannels.h"

const FS2MoleTier ADRS2MoleGame::DefaultTier = FS2MoleTier();

ADRS2MoleGame::ADRS2MoleGame()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));

	// Plan6 §14.4.3 표 기본값
	Tiers.Add({ 0,  2.0f, 0.8f });
	Tiers.Add({ 5,  1.3f, 0.5f });
	Tiers.Add({ 12, 0.8f, 0.3f });
}

void ADRS2MoleGame::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2MoleGame, KillCount);
	DOREPLIFETIME(ADRS2MoleGame, bActive);
}

void ADRS2MoleGame::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && SpawnPoints.Num() == 0)
	{
		UE_LOG(LogDR, Error, TEXT("[S2MoleGame] 등장 지점이 배선되지 않았습니다. 두더지가 스폰되지 않습니다."));
	}
}

void ADRS2MoleGame::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSpawning();
	Super::EndPlay(EndPlayReason);
}

void ADRS2MoleGame::StartGame()
{
	if (!HasAuthority()) return;

	// 재시도 시에도 항상 0부터 시작한다 (진행도 누적 없음)
	DestroyAllMoles();

	KillCount = 0;
	bActive = true;
	OnRep_KillCount();

	OnProgress.Broadcast(KillCount, GoalKills);

	RestartSpawnTimer();

	UE_LOG(LogDR, Log, TEXT("[S2MoleGame] 시작 - 목표 %d마리"), GoalKills);
}

void ADRS2MoleGame::AbortAndReset()
{
	if (!HasAuthority()) return;

	StopSpawning();
	DestroyAllMoles();

	KillCount = 0;
	bActive = false;
	OnRep_KillCount();

	OnProgress.Broadcast(KillCount, GoalKills);

	UE_LOG(LogDR, Log, TEXT("[S2MoleGame] 중단 및 초기화"));
}

const FS2MoleTier& ADRS2MoleGame::GetCurrentTier() const
{
	if (Tiers.Num() == 0) return DefaultTier;

	// KillThreshold 내림차순으로 첫 매치를 찾는다
	for (int32 i = Tiers.Num() - 1; i >= 0; --i)
	{
		if (KillCount >= Tiers[i].KillThreshold)
		{
			return Tiers[i];
		}
	}

	return Tiers[0];
}

void ADRS2MoleGame::RestartSpawnTimer()
{
	if (!HasAuthority() || !bActive) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const FS2MoleTier& Tier = GetCurrentTier();

	// 티어가 바뀌면 새 간격으로 다시 건다.
	// 이미 나와 있는 두더지의 잔여 유지 시간은 갱신하지 않는다 (Plan6 §14.4.3).
	World->GetTimerManager().SetTimer(
		SpawnTimer, this, &ADRS2MoleGame::SpawnMole, Tier.SpawnInterval, true);
}

void ADRS2MoleGame::StopSpawning()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimer);
	}
}

void ADRS2MoleGame::SpawnMole()
{
	if (!HasAuthority() || !bActive) return;

	// 무효 참조 정리
	ActiveMoles.RemoveAll([](const TWeakObjectPtr<ADRS2Mole>& Ptr) { return !Ptr.IsValid(); });

	if (ActiveMoles.Num() >= MaxConcurrentMoles) return;
	if (!MoleClass || SpawnPoints.Num() == 0) return;

	// 직전 지점 연속 선택 회피
	int32 PointIndex = FMath::RandRange(0, SpawnPoints.Num() - 1);
	if (SpawnPoints.Num() > 1 && PointIndex == LastSpawnPointIndex)
	{
		PointIndex = (PointIndex + 1) % SpawnPoints.Num();
	}
	LastSpawnPointIndex = PointIndex;

	const AActor* SpawnPoint = SpawnPoints[PointIndex];
	if (!IsValid(SpawnPoint)) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADRS2Mole* Mole = GetWorld()->SpawnActor<ADRS2Mole>(
		MoleClass, SpawnPoint->GetActorTransform(), SpawnParams);
	if (!Mole) return;

	const FS2MoleTier& Tier = GetCurrentTier();
	Mole->InitFromGame(this, Tier.MoleLifetime);

	ActiveMoles.Add(Mole);
}

void ADRS2MoleGame::OnMoleKilled(ADRS2Mole* Mole)
{
	if (!HasAuthority() || !bActive) return;

	ActiveMoles.RemoveAll([Mole](const TWeakObjectPtr<ADRS2Mole>& Ptr)
	{
		return !Ptr.IsValid() || Ptr.Get() == Mole;
	});

	const int32 PreviousThreshold = GetCurrentTier().KillThreshold;

	++KillCount;
	OnRep_KillCount();
	OnProgress.Broadcast(KillCount, GoalKills);

	if (KillCount >= GoalKills)
	{
		bActive = false;
		StopSpawning();

		UE_LOG(LogDR, Log, TEXT("[S2MoleGame] 클리어 - %d마리 달성"), KillCount);

		OnCleared.Broadcast();
		return;
	}

	// 티어가 바뀌었으면 다음 스폰부터 새 간격을 적용한다
	if (GetCurrentTier().KillThreshold != PreviousThreshold)
	{
		RestartSpawnTimer();
	}
}

void ADRS2MoleGame::OnMoleExpired(ADRS2Mole* Mole)
{
	if (!HasAuthority()) return;

	// 놓쳐도 패널티는 없다. 목록에서만 제거한다.
	ActiveMoles.RemoveAll([Mole](const TWeakObjectPtr<ADRS2Mole>& Ptr)
	{
		return !Ptr.IsValid() || Ptr.Get() == Mole;
	});
}

void ADRS2MoleGame::DestroyAllMoles()
{
	for (const TWeakObjectPtr<ADRS2Mole>& MolePtr : ActiveMoles)
	{
		if (MolePtr.IsValid())
		{
			MolePtr->Destroy();
		}
	}

	ActiveMoles.Reset();
}

void ADRS2MoleGame::OnRep_KillCount()
{
	OnKillCountChanged(KillCount, GoalKills);
}
