// Copyright DaeRune

#include "Phase/Stage2/DRS2CombatPhase.h"

#include "Actor/Stage2/DRS2StageDirector.h"
#include "Actor/Stage2/DRS2RoomTrigger.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "TimerManager.h"
#include "DaeRune/DRLogChannels.h"

void UDRS2CombatPhase::OnPhaseStart()
{
	Super::OnPhaseStart();

	if (!GameMode || !GameState) return;

	ADRS2StageDirector* Director = GetDirector();
	if (!Director) return;

	SetupPhaseObjectiveByRow(TEXT("S2P1_Enter"));

	if (!Director->Trigger_Room1)
	{
		UE_LOG(LogDR, Error, TEXT("[S2P1] Trigger_Room1 미배선 - 방1 진행 불가"));
	}
	ArmRoomTrigger(Director->Trigger_Room1);
}

void UDRS2CombatPhase::OnRoomAllInside()
{
	if (bCombatStarted) return;

	ADRS2StageDirector* Director = GetDirector();
	if (!Director || !GameMode || !GameState) return;

	// ① 기준 인원 확정 (이후 사망해도 재계산하지 않는다)
	ResolveBasePlayerCount();

	// ② 시작지점 통로를 구조물로 영구 봉쇄
	SetBlockerBlocked(Director->Blocker_StartToRoom1, true);

	// ③ 레벨에 배치된 스폰 지점 수집 (Director 배선이 있으면 그쪽 우선)
	InitSpawnPoints(SpawnPointRoomID, Director->Room1SpawnPoints);

	// ④ 인원 구간에 맞는 웨이브 세트 조회
	const FS2WaveSet* WaveSet = ResolveWaveSet(WaveSetsByPlayerCount);
	if (!WaveSet)
	{
		// 웨이브 구성이 없으면 방이 영원히 끝나지 않는다. 진행을 막지 않도록 즉시 완료 처리한다.
		UE_LOG(LogDR, Error, TEXT("[S2P1] 웨이브 구성이 없어 전투를 건너뜁니다. BP_S2CombatPhase 설정을 확인하세요."));
		bCombatStarted = true;
		GameMode->ValidatePhaseCompletion();
		return;
	}

	TotalSpawnCount = CountTotalSpawns(*WaveSet);
	PendingSpawnCount = TotalSpawnCount;
	KilledCount = 0;

	if (TotalSpawnCount <= 0)
	{
		UE_LOG(LogDR, Error, TEXT("[S2P1] 웨이브 마리 수가 0입니다. 전투를 건너뜁니다."));
		bCombatStarted = true;
		GameMode->ValidatePhaseCompletion();
		return;
	}

	// ⑤ 목표 교체 - 분모는 런타임에 확정된 총 스폰 수
	SetupPhaseObjectiveByRow(TEXT("S2P1_Combat"), TotalSpawnCount);
	GameState->UpdatePhaseObjectiveProgress(0);

	// ⑥ 웨이브 예약 (0초 / 30초). 앞 웨이브를 먼저 전멸시키면 OnEnemyDeath 가 다음 웨이브를 앞당긴다.
	const int32 NumWaves = WaveSet->Waves.Num();
	WaveTimers.SetNum(NumWaves);
	WaveStarted.Init(false, NumWaves);
	WaveSpawnCounts.SetNum(NumWaves);
	UnstartedSpawnCount = 0;
	for (int32 WaveIndex = 0; WaveIndex < NumWaves; ++WaveIndex)
	{
		WaveSpawnCounts[WaveIndex] = CountComposition(WaveSet->Waves[WaveIndex]);
		UnstartedSpawnCount += WaveSpawnCounts[WaveIndex];
	}

	bCombatStarted = true;
	for (int32 WaveIndex = 0; WaveIndex < NumWaves; ++WaveIndex)
	{
		const float Delay = WaveSet->Waves[WaveIndex].StartDelaySeconds;
		if (Delay <= 0.f)
		{
			StartWave(WaveIndex);
		}
		else if (UWorld* World = GetPhaseWorld())
		{
			World->GetTimerManager().SetTimer(WaveTimers[WaveIndex],
				FTimerDelegate::CreateUObject(this, &UDRS2CombatPhase::StartWave, WaveIndex), Delay, false);
		}
	}

	UE_LOG(LogDR, Log, TEXT("[S2P1] 전투 시작 - 기준 인원 %d, 총 %d마리, 스폰 지점 %d개"),
		BasePlayerCount, TotalSpawnCount, GetSpawnPointCount());
}

void UDRS2CombatPhase::StartWave(int32 WaveIndex)
{
	if (!WaveStarted.IsValidIndex(WaveIndex) || WaveStarted[WaveIndex]) return;

	const FS2WaveSet* WaveSet = ResolveWaveSet(WaveSetsByPlayerCount);
	if (!WaveSet || !WaveSet->Waves.IsValidIndex(WaveIndex)) return;

	// ★스폰보다 먼저 표시한다 — 스폰 도중 적이 죽어 OnEnemyDeath 가 다시 들어와도 같은 웨이브를 또 내지 않게
	WaveStarted[WaveIndex] = true;
	UnstartedSpawnCount -= WaveSpawnCounts[WaveIndex];
	if (UWorld* World = GetPhaseWorld())
	{
		World->GetTimerManager().ClearTimer(WaveTimers[WaveIndex]);
	}

	UE_LOG(LogDR, Log, TEXT("[S2P1] 웨이브 %d/%d 스폰 (%d마리)"), WaveIndex + 1, WaveStarted.Num(), WaveSpawnCounts[WaveIndex]);

	SpawnComposition(WaveSet->Waves[WaveIndex]);
}

int32 UDRS2CombatPhase::FindNextUnstartedWave() const
{
	return WaveStarted.IndexOfByKey(false);
}

void UDRS2CombatPhase::OnEnemyDeath(AActor* DeadEnemy)
{
	// 베이스가 SpawnedEnemies 에서 제거한다
	Super::OnEnemyDeath(DeadEnemy);

	if (!bCombatStarted || !GameMode || !GameState) return;

	++KilledCount;
	GameState->UpdatePhaseObjectiveProgress(KilledCount);

	// 나온 적이 다 죽었는지: 살아있는 적이 없고, 나오는 중인(지연 스폰) 적도 없어야 한다.
	// PendingSpawnCount 에는 아직 안 나온 웨이브 몫(UnstartedSpawnCount)도 들어 있다.
	if (GetAliveEnemyCount() > 0 || PendingSpawnCount > UnstartedSpawnCount) return;

	// 남은 웨이브가 있으면 예약 시각을 기다리지 않고 바로 내보낸다
	const int32 NextWave = FindNextUnstartedWave();
	if (NextWave != INDEX_NONE)
	{
		UE_LOG(LogDR, Log, TEXT("[S2P1] 나온 적 전멸 - 웨이브 %d 즉시 스폰"), NextWave + 1);
		StartWave(NextWave);
		return;
	}

	if (PendingSpawnCount == 0)
	{
		UE_LOG(LogDR, Log, TEXT("[S2P1] 전멸 - 방1 완료 (%d/%d)"), KilledCount, TotalSpawnCount);
		GameMode->ValidatePhaseCompletion();
	}
}

bool UDRS2CombatPhase::IsCompleted() const
{
	return bCombatStarted && GetAliveEnemyCount() == 0 && PendingSpawnCount == 0;
}

void UDRS2CombatPhase::OnPhaseEnd()
{
	if (UWorld* World = GetPhaseWorld())
	{
		for (FTimerHandle& WaveTimer : WaveTimers)
		{
			World->GetTimerManager().ClearTimer(WaveTimer);
		}
	}

	if (ADRS2StageDirector* Director = GetDirector())
	{
		DisarmRoomTrigger(Director->Trigger_Room1);
	}

	// D1 게이트 발광은 여기서 하지 않는다.
	// 게임오버도 OnPhaseEnd 를 호출하므로, 진행성 부수효과는 다음 페이즈의 OnPhaseStart 가 담당한다.

	Super::OnPhaseEnd();
}
