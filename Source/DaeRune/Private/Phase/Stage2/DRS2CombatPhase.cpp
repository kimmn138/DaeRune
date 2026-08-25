// Copyright DaeRune

#include "Phase/Stage2/DRS2CombatPhase.h"

#include "Actor/Stage2/DRS2StageDirector.h"
#include "Actor/Stage2/DRS2RoomTrigger.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "DaeRune/DRLogChannels.h"

void UDRS2CombatPhase::OnPhaseStart()
{
	Super::OnPhaseStart();

	if (!GameMode || !GameState) return;

	ADRS2StageDirector* Director = GetDirector();
	if (!Director) return;

	SetupPhaseObjectiveByRow(TEXT("S2P1_Enter"));

	if (ADRS2RoomTrigger* Trigger = Director->Trigger_Room1)
	{
		Trigger->OnAllInside.AddDynamic(this, &UDRS2CombatPhase::HandleAllInsideRoom1);
		Trigger->OnCountChanged.AddDynamic(this, &UDRS2CombatPhase::HandleInsideCountChanged);
		Trigger->Arm();
	}
	else
	{
		UE_LOG(LogDR, Error, TEXT("[S2P1] Trigger_Room1 미배선 - 방1 진행 불가"));
	}
}

void UDRS2CombatPhase::HandleInsideCountChanged(int32 InsideCount, int32 /*AliveTotal*/)
{
	// 전투가 시작된 뒤에는 진행도가 "처치 수"이므로 입장 인원으로 덮어쓰지 않는다.
	if (bCombatStarted || !GameState) return;

	GameState->UpdatePhaseObjectiveProgress(InsideCount);
}

void UDRS2CombatPhase::HandleAllInsideRoom1()
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

	// ⑥ 웨이브 예약 (0초 / 30초). 웨이브1 전멸 여부와 무관하게 시간 기반으로 진행된다.
	bCombatStarted = true;
	ScheduleWaveSet(*WaveSet);

	UE_LOG(LogDR, Log, TEXT("[S2P1] 전투 시작 - 기준 인원 %d, 총 %d마리, 스폰 지점 %d개"),
		BasePlayerCount, TotalSpawnCount, GetSpawnPointCount());
}

void UDRS2CombatPhase::OnEnemyDeath(AActor* DeadEnemy)
{
	// 베이스가 SpawnedEnemies 에서 제거한다
	Super::OnEnemyDeath(DeadEnemy);

	if (!bCombatStarted || !GameMode || !GameState) return;

	++KilledCount;
	GameState->UpdatePhaseObjectiveProgress(KilledCount);

	// 전멸 판정: 살아있는 적이 없고, 예약된 스폰도 남아있지 않아야 한다.
	// 웨이브1을 30초 안에 전멸시킨 경우 PendingSpawnCount 가 웨이브2 분만큼 남아 있어 여기서 걸러진다.
	if (GetAliveEnemyCount() == 0 && PendingSpawnCount == 0)
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
	if (ADRS2StageDirector* Director = GetDirector())
	{
		if (ADRS2RoomTrigger* Trigger = Director->Trigger_Room1)
		{
			Trigger->OnAllInside.RemoveDynamic(this, &UDRS2CombatPhase::HandleAllInsideRoom1);
			Trigger->OnCountChanged.RemoveDynamic(this, &UDRS2CombatPhase::HandleInsideCountChanged);
			Trigger->Disarm();
		}
	}

	// D1 게이트 발광은 여기서 하지 않는다.
	// 게임오버도 OnPhaseEnd 를 호출하므로, 진행성 부수효과는 다음 페이즈의 OnPhaseStart 가 담당한다.

	Super::OnPhaseEnd();
}
