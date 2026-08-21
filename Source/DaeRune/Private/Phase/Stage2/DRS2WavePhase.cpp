// Copyright DaeRune

#include "Phase/Stage2/DRS2WavePhase.h"

#include "Actor/Stage2/DRS2StageDirector.h"
#include "Actor/Stage2/DRS2RoomTrigger.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "TimerManager.h"
#include "DaeRune/DRLogChannels.h"

void UDRS2WavePhase::OnPhaseStart()
{
	Super::OnPhaseStart();

	if (!GameMode || !GameState) return;

	ADRS2StageDirector* Director = GetDirector();
	if (!Director) return;

	// D4 하강 개방. 방3 페이즈의 클리어 처리에서 이미 열렸을 수 있으나
	// SetBlocked 가 멱등이므로 같은 값이면 no-op 이다 (§4.11).
	SetBlockerBlocked(Director->Blocker_Room3ToRoom5, false);

	SetupPhaseObjectiveByRow(TEXT("S2P4_Enter"));

	if (ADRS2RoomTrigger* Trigger = Director->Trigger_Room5)
	{
		Trigger->OnAllInside.AddDynamic(this, &UDRS2WavePhase::HandleAllInsideRoom5);
		Trigger->OnCountChanged.AddDynamic(this, &UDRS2WavePhase::HandleInsideCountChanged);
		Trigger->Arm();
	}
	else
	{
		UE_LOG(LogDR, Error, TEXT("[S2P4] Trigger_Room5 미배선 - 방5 진행 불가"));
	}
}

void UDRS2WavePhase::HandleInsideCountChanged(int32 InsideCount, int32 /*AliveTotal*/)
{
	// 웨이브가 시작된 뒤에는 진행도가 "현재 웨이브 번호"이므로 입장 인원으로 덮어쓰지 않는다.
	if (bWavesStarted || !GameState) return;

	GameState->UpdatePhaseObjectiveProgress(InsideCount);
}

void UDRS2WavePhase::HandleAllInsideRoom5()
{
	if (bWavesStarted) return;

	ADRS2StageDirector* Director = GetDirector();
	if (!Director || !GameMode || !GameState) return;

	// ① 기준 인원 확정 (이후 사망해도 재계산하지 않는다)
	ResolveBasePlayerCount();

	// ② D4 재봉쇄 - 방3으로 돌아갈 수 없다 (§14.5.4, 구조물의 유일한 왕복 동작)
	SetBlockerBlocked(Director->Blocker_Room3ToRoom5, true);

	// ③ 스폰 지점 수집
	InitSpawnPoints(SpawnPointRoomID, Director->Room5SpawnPoints);

	// ④ 웨이브 세트 확인
	const FS2WaveSet* WaveSet = ResolveWaveSet(WaveSetsByPlayerCount);
	if (!WaveSet || WaveSet->Waves.Num() == 0)
	{
		// 구성이 없으면 방이 영원히 끝나지 않는다. 진행을 막지 않도록 즉시 완료 처리한다.
		UE_LOG(LogDR, Error, TEXT("[S2P4] 웨이브 구성이 없어 방5를 건너뜁니다. BP_S2WavePhase 설정을 확인하세요."));
		bWavesStarted = true;
		TotalWaveCount = 0;
		CurrentWaveIndex = INDEX_NONE;
		GameMode->ValidatePhaseCompletion();
		return;
	}

	TotalWaveCount = WaveSet->Waves.Num();
	if (TotalWaveCount != 3)
	{
		UE_LOG(LogDR, Warning, TEXT("[S2P4] 웨이브가 %d개입니다. 사양은 3개입니다."), TotalWaveCount);
	}

	// ⑤ 기존 웨이브 번호 UI 복제 필드 재활용
	GameState->SetTotalWaves(TotalWaveCount);

	bWavesStarted = true;
	StartWave(0);

	UE_LOG(LogDR, Log, TEXT("[S2P4] 방5 시작 - 기준 인원 %d, 웨이브 %d개, 스폰 지점 %d개"),
		BasePlayerCount, TotalWaveCount, GetSpawnPointCount());
}

void UDRS2WavePhase::StartWave(int32 WaveIndex)
{
	if (!GameMode || !GameState) return;

	const FS2WaveSet* WaveSet = ResolveWaveSet(WaveSetsByPlayerCount);
	if (!WaveSet || !WaveSet->Waves.IsValidIndex(WaveIndex)) return;

	CurrentWaveIndex = WaveIndex;

	GameState->SetCurrentWaveNumber(WaveIndex + 1);
	SetupPhaseObjectiveByRow(TEXT("S2P4_Wave"), TotalWaveCount);
	GameState->UpdatePhaseObjectiveProgress(WaveIndex + 1);
	GameState->Multicast_PlayWaveStartSound();

	const FS2WaveComposition& Composition = WaveSet->Waves[WaveIndex];
	PendingSpawnCount += CountComposition(Composition);
	SpawnComposition(Composition);

	// ★마지막 웨이브에는 다음 웨이브 타이머를 걸지 않는다 (4번째 웨이브 방지)
	if (WaveIndex < TotalWaveCount - 1)
	{
		if (UWorld* World = GetPhaseWorld())
		{
			FTimerDelegate Delegate = FTimerDelegate::CreateUObject(
				this, &UDRS2WavePhase::StartWave, WaveIndex + 1);

			World->GetTimerManager().SetTimer(NextWaveTimer, Delegate, WaveIntervalSeconds, false);
		}
	}

	UE_LOG(LogDR, Log, TEXT("[S2P4] 웨이브 %d/%d 스폰 (대기 %d마리)"),
		WaveIndex + 1, TotalWaveCount, PendingSpawnCount);
}

void UDRS2WavePhase::OnEnemyDeath(AActor* DeadEnemy)
{
	// 베이스가 SpawnedEnemies 에서 제거한다
	Super::OnEnemyDeath(DeadEnemy);

	if (!bWavesStarted || !GameMode) return;

	// ★판정 기준은 웨이브 단위가 아니라 전체 생존 수다.
	//   30초 타이머로 웨이브가 겹쳐 스폰된 경우 혼재분까지 모두 전멸해야 다음으로 넘어간다.
	if (GetAliveEnemyCount() > 0 || PendingSpawnCount > 0) return;

	if (!IsLastWave())
	{
		// 30초를 기다리지 않고 즉시 다음 웨이브로 (하이브리드 전환)
		if (UWorld* World = GetPhaseWorld())
		{
			World->GetTimerManager().ClearTimer(NextWaveTimer);
		}

		UE_LOG(LogDR, Log, TEXT("[S2P4] 웨이브 %d 전멸 - 즉시 다음 웨이브"), CurrentWaveIndex + 1);

		StartWave(CurrentWaveIndex + 1);
	}
	else
	{
		UE_LOG(LogDR, Log, TEXT("[S2P4] 마지막 웨이브 전멸 - 방5 완료"));

		GameMode->ValidatePhaseCompletion();
	}
}

bool UDRS2WavePhase::IsCompleted() const
{
	return bWavesStarted
		&& IsLastWave()
		&& GetAliveEnemyCount() == 0
		&& PendingSpawnCount == 0;
}

void UDRS2WavePhase::OnPhaseEnd()
{
	if (UWorld* World = GetPhaseWorld())
	{
		World->GetTimerManager().ClearTimer(NextWaveTimer);
	}

	if (ADRS2StageDirector* Director = GetDirector())
	{
		if (ADRS2RoomTrigger* Trigger = Director->Trigger_Room5)
		{
			Trigger->OnAllInside.RemoveDynamic(this, &UDRS2WavePhase::HandleAllInsideRoom5);
			Trigger->OnCountChanged.RemoveDynamic(this, &UDRS2WavePhase::HandleInsideCountChanged);
			Trigger->Disarm();
		}
	}

	// D5(방5->방6) 하강 개방은 방6 페이즈의 OnPhaseStart 담당이다.
	// 게임오버도 OnPhaseEnd 를 호출하므로 진행성 부수효과를 여기에 두지 않는다.

	Super::OnPhaseEnd();
}
