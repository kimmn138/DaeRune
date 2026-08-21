// Copyright DaeRune

#include "Phase/Stage2/DRS2DefensePhase.h"

#include "Actor/DRCleanserPart.h"
#include "Actor/DRCleanserSite.h"
#include "Actor/Stage2/DRS2StageDirector.h"
#include "Actor/Stage2/DRS2RoomTrigger.h"
#include "Actor/Stage2/DRS2TeleportGate.h"
#include "Actor/Stage2/DRS2MoleGame.h"
#include "Character/DRCharacter.h"
#include "EngineUtils.h"
#include "Game/DRGameStateBase.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/CombatInterface.h"
#include "TimerManager.h"
#include "DaeRune/DRLogChannels.h"

void UDRS2DefensePhase::OnPhaseStart()
{
	Super::OnPhaseStart();

	if (!GameMode || !GameState) return;

	ADRS2StageDirector* Director = GetDirector();
	if (!Director) return;

	SetupPhaseObjectiveByRow(TEXT("S2P3_Enter"));

	if (ADRS2RoomTrigger* Trigger = Director->Trigger_Room3)
	{
		Trigger->OnAllInside.AddDynamic(this, &UDRS2DefensePhase::HandleAllInsideRoom3);
		Trigger->OnCountChanged.AddDynamic(this, &UDRS2DefensePhase::HandleInsideCountChanged);
		Trigger->Arm();
	}
	else
	{
		UE_LOG(LogDR, Error, TEXT("[S2P3] Trigger_Room3 미배선 - 방3 진행 불가"));
	}

	// 비정상 진입(페이즈 스킵 치트 등) 대비 안전망
	EnsurePartExists(Director);
}

void UDRS2DefensePhase::EnsurePartExists(ADRS2StageDirector* Director)
{
	UWorld* World = GetPhaseWorld();
	if (!World || !Director) return;

	// 정상 흐름에서는 방2를 완료(부품 소지자 퇴장)해야 방3에 오므로 부품이 반드시 존재한다.
	for (TActorIterator<ADRCleanserPart> It(World); It; ++It)
	{
		if (IsValid(*It)) return;
	}

	// 여기까지 왔다면 월드에 부품이 하나도 없다.
	// 그대로 두면 IsPartPresentInRoom3 가 영원히 실패해 방3이 시작되지 않는다.
	if (!PartClass)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2P3] 월드에 부품이 없고 PartClass 도 비어 있습니다. 방3을 시작할 수 없습니다."));
		return;
	}

	const AActor* SpawnPoint = Director->Room4EntranceDropPoint;
	const FVector SpawnLocation = SpawnPoint
		? SpawnPoint->GetActorLocation()
		: (Director->Trigger_Room3 ? Director->Trigger_Room3->GetActorLocation() : FVector::ZeroVector);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	World->SpawnActor<ADRCleanserPart>(PartClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

	UE_LOG(LogDR, Warning,
		TEXT("[S2P3] 월드에 부품이 없어 안전망으로 방3에 부품을 생성했습니다 (페이즈 스킵 등 비정상 진입)."));
}

void UDRS2DefensePhase::HandleInsideCountChanged(int32 InsideCount, int32 /*AliveTotal*/)
{
	// 방이 시작된 뒤에는 진행도가 "두더지 처치 수"이므로 입장 인원으로 덮어쓰지 않는다.
	if (bRoomStarted || !GameState) return;

	GameState->UpdatePhaseObjectiveProgress(InsideCount);
}

bool UDRS2DefensePhase::IsPartPresentInRoom3(ADRS2StageDirector* Director) const
{
	UWorld* World = GetPhaseWorld();
	if (!World || !Director || !Director->Trigger_Room3) return false;

	for (TActorIterator<ADRCleanserPart> It(World); It; ++It)
	{
		ADRCleanserPart* Part = *It;
		if (!IsValid(Part)) continue;

		// 누군가 들고 있거나, 방3 트리거 안에 놓여 있으면 통과
		if (Part->IsCarriedNow() || Director->Trigger_Room3->IsActorInside(Part))
		{
			return true;
		}
	}

	return false;
}

void UDRS2DefensePhase::HandleAllInsideRoom3()
{
	if (bRoomStarted) return;

	ADRS2StageDirector* Director = GetDirector();
	if (!Director || !GameMode || !GameState) return;

	// ★부품 동반 검증. 부품 없이 봉쇄하면 진행이 불가능해지므로 발화를 무시하고 재무장한다.
	if (!IsPartPresentInRoom3(Director))
	{
		UE_LOG(LogDR, Log, TEXT("[S2P3] 부품 없이 전원 입장 - 봉쇄 보류 후 재무장"));
		if (Director->Trigger_Room3)
		{
			Director->Trigger_Room3->ReArm();
		}
		return;
	}

	bRoomStarted = true;

	// ① 기준 인원 확정 (이후 사망해도 재계산하지 않는다)
	ResolveBasePlayerCount();

	// ② 방1 통로를 구조물로 영구 봉쇄
	SetBlockerBlocked(Director->Blocker_Room1ToRoom3, true);

	// ③ 방3 스폰 지점 수집
	InitSpawnPoints(SpawnPointRoomID, Director->Room3SpawnPoints);

	// ④ 방4 문 발광 + 부품 소지자만 통과
	if (ADRS2TeleportGate* Gate = Director->Gate_Room3ToRoom4)
	{
		Gate->SetEntryRule(ES2GateEntryRule::CarrierOnly);
		Gate->OnGateUsed.AddDynamic(this, &UDRS2DefensePhase::HandleRoom4Entered);
		Gate->SetGateActive(true);
	}

	// ⑤ 설치대 / 두더지 게임 바인딩
	if (ADRCleanserSite* Site = Director->Room4InstallSite)
	{
		Site->OnPartInstalled.AddDynamic(this, &UDRS2DefensePhase::HandlePartInstalled);
	}
	if (ADRS2MoleGame* MoleGame = Director->Room4MoleGame)
	{
		MoleGame->OnProgress.AddDynamic(this, &UDRS2DefensePhase::HandleMoleProgress);
		MoleGame->OnCleared.AddDynamic(this, &UDRS2DefensePhase::HandleMoleGameCleared);
	}

	SetupPhaseObjectiveByRow(TEXT("S2P3_Enter4"));

	UE_LOG(LogDR, Log, TEXT("[S2P3] 방3 시작 - 기준 인원 %d"), BasePlayerCount);
}

void UDRS2DefensePhase::HandleRoom4Entered(ADRCharacter* Who)
{
	ADRS2StageDirector* Director = GetDirector();
	if (!Director) return;

	Room4Player = Who;

	// 1명만 들어간다. 게이트는 bDeactivateOnUse 로 이미 잠겼지만 명시적으로 한 번 더 보장한다.
	SetGateActive(Director->Gate_Room3ToRoom4, false);

	SetupPhaseObjectiveByRow(TEXT("S2P3_Hold"), 20);

	UE_LOG(LogDR, Log, TEXT("[S2P3] 방4 입장: %s"), Who ? *Who->GetName() : TEXT("Unknown"));
}

void UDRS2DefensePhase::HandlePartInstalled(ADRCleanserSite* Site)
{
	ADRS2StageDirector* Director = GetDirector();
	if (!Director || !GameState) return;

	// 두더지 게임과 방3 웨이브를 동시에 시작한다 (Plan6 §14.3.2).
	// 재시도(예외 A/B) 후 재설치에도 같은 경로를 타므로 웨이브가 함께 재개된다.
	if (ADRS2MoleGame* MoleGame = Director->Room4MoleGame)
	{
		MoleGame->StartGame();
	}

	StartWaveLoop();

	const int32 GoalKills = Director->Room4MoleGame ? Director->Room4MoleGame->GetGoalKills() : 20;
	SetupPhaseObjectiveByRow(TEXT("S2P3_Hold"), GoalKills);
	GameState->UpdatePhaseObjectiveProgress(0);

	UE_LOG(LogDR, Log, TEXT("[S2P3] 부품 설치 - 두더지 + 방3 웨이브 시작"));
}

// ================= 방3 웨이브 루프 =================

void UDRS2DefensePhase::StartWaveLoop()
{
	UWorld* World = GetPhaseWorld();
	if (!World) return;

	// 첫 웨이브는 즉시 (Plan6 §14.3.6-3)
	SpawnOneWave();

	World->GetTimerManager().SetTimer(
		WaveLoopTimer, this, &UDRS2DefensePhase::SpawnOneWave, WaveIntervalSeconds, true);
}

void UDRS2DefensePhase::StopWaveLoop()
{
	if (UWorld* World = GetPhaseWorld())
	{
		World->GetTimerManager().ClearTimer(WaveLoopTimer);
	}
}

void UDRS2DefensePhase::SpawnOneWave()
{
	// 안전장치: 동시 생존 상한에 도달하면 이번 웨이브를 건너뛴다 (게임오버가 아니다)
	if (GetAliveEnemyCount() >= MaxAliveEnemies)
	{
		UE_LOG(LogDR, Warning, TEXT("[S2P3] 동시 생존 상한(%d) 도달 - 이번 웨이브 스킵"), MaxAliveEnemies);
		return;
	}

	const FS2WaveSet* WaveSet = ResolveWaveSet(WaveSetsByPlayerCount);
	if (!WaveSet || WaveSet->Waves.Num() == 0)
	{
		UE_LOG(LogDR, Error, TEXT("[S2P3] 웨이브 구성이 없습니다. BP_S2DefensePhase 설정을 확인하세요."));
		StopWaveLoop();
		return;
	}

	// 무한 웨이브: 구성 1개를 반복해서 스폰한다
	const FS2WaveComposition& Composition = WaveSet->Waves[0];
	PendingSpawnCount += CountComposition(Composition);
	SpawnComposition(Composition);
}

// ================= 클리어 처리 =================

void UDRS2DefensePhase::HandleMoleProgress(int32 KillCount, int32 /*GoalKills*/)
{
	if (!GameState) return;

	// 방3 인원에게도 두더지 진행도를 공유한다 (Plan6 §14.4.5-8)
	GameState->UpdatePhaseObjectiveProgress(KillCount);
}

void UDRS2DefensePhase::HandleMoleGameCleared()
{
	if (bMoleGameCleared || !GameMode) return;

	ADRS2StageDirector* Director = GetDirector();
	if (!Director) return;

	// ① 방4 복귀 게이트 활성 + D3 재활성
	SetGateActive(Director->Gate_Room4Return, true);
	if (ADRS2TeleportGate* Gate = Director->Gate_Room3ToRoom4)
	{
		Gate->SetEntryRule(ES2GateEntryRule::Anyone);
		Gate->SetGateActive(true);
	}

	// ② 사망자 전원 부활 (방3에서 체력 50%)
	ReviveAllDeadPlayers(Director);

	// ③ 방5 통로 하강 개방
	SetBlockerBlocked(Director->Blocker_Room3ToRoom5, false);

	// ④ 스폰 중단 + 잔적 즉시 사망
	StopWaveLoop();
	PendingSpawnCount = 0;
	KillAllSpawnedEnemies();

	bMoleGameCleared = true;

	UE_LOG(LogDR, Log, TEXT("[S2P3] 두더지 클리어 - 부활/개방/정리 완료"));

	GameMode->ValidatePhaseCompletion();
}

void UDRS2DefensePhase::ReviveAllDeadPlayers(ADRS2StageDirector* Director)
{
	UWorld* World = GetPhaseWorld();
	if (!World || !Director) return;

	if (Director->Room3RevivePoints.Num() == 0)
	{
		UE_LOG(LogDR, Error, TEXT("[S2P3] 부활 지점이 없어 부활을 건너뜁니다."));
		return;
	}

	int32 Slot = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		ADRCharacter* Character = It->IsValid() ? Cast<ADRCharacter>(It->Get()->GetPawn()) : nullptr;
		if (!Character) continue;

		// IsDead 는 ICombatInterface 의 BlueprintNativeEvent
		if (!ICombatInterface::Execute_IsDead(Character)) continue;

		const int32 PointIndex = Director->Room3RevivePoints.IsValidIndex(Slot) ? Slot : 0;
		const AActor* RevivePoint = Director->Room3RevivePoints[PointIndex];
		if (!IsValid(RevivePoint)) continue;

		Character->Revive(RevivePoint->GetActorLocation(), ReviveHealthRatio, ReviveWaterRatio);
		++Slot;

		UE_LOG(LogDR, Log, TEXT("[S2P3] 부활: %s"), *Character->GetName());
	}
}

void UDRS2DefensePhase::KillAllSpawnedEnemies()
{
	// 역순 순회 - OnEnemyDeath 콜백이 SpawnedEnemies 를 수정한다
	for (int32 i = SpawnedEnemies.Num() - 1; i >= 0; --i)
	{
		if (!SpawnedEnemies.IsValidIndex(i) || !SpawnedEnemies[i].IsValid()) continue;

		AActor* Enemy = SpawnedEnemies[i].Get();
		if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Enemy))
		{
			// 사망 경로: 사망 연출과 물 보상이 발생한다
			CombatInterface->Die(FVector::ZeroVector);
		}
	}
}

void UDRS2DefensePhase::DestroyAllSpawnedEnemies()
{
	// 조용히 제거 (연출/보상 없음). 사망 델리게이트도 해제한다.
	for (int32 i = SpawnedEnemies.Num() - 1; i >= 0; --i)
	{
		if (!SpawnedEnemies.IsValidIndex(i) || !SpawnedEnemies[i].IsValid()) continue;

		AActor* Enemy = SpawnedEnemies[i].Get();
		if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Enemy))
		{
			CombatInterface->GetOnDeathDelegate().RemoveDynamic(this, &UDRPhaseBase::OnEnemyDeath);
		}
		Enemy->Destroy();
	}

	SpawnedEnemies.Empty();
}

// ================= 예외 처리 (Plan6 §14.3.5) =================

void UDRS2DefensePhase::PrepareRoom4Retry(ADRS2StageDirector* Director, bool bEjectPartInsideRoom4)
{
	if (!Director) return;

	// ① 두더지 전부 제거 + 처치 수 0으로 초기화
	if (ADRS2MoleGame* MoleGame = Director->Room4MoleGame)
	{
		MoleGame->AbortAndReset();
	}

	// ② 방3 스폰 중단 + 남은 몬스터를 전부 제거한다.
	//    재설치로 두더지 게임이 다시 시작될 때까지 웨이브를 재개하지 않는다 (2026-08-07 수정).
	//    사망/이탈 어느 경로든 동일하게 "그냥 제거"한다 (사망 연출·물 보상 없음).
	StopWaveLoop();
	PendingSpawnCount = 0;
	DestroyAllSpawnedEnemies();

	// ③ 부품을 되돌린다
	ADRCleanserPart* EjectedPart = nullptr;
	if (ADRCleanserSite* Site = Director->Room4InstallSite)
	{
		EjectedPart = Site->EjectInstalledPart(PartClass);
	}

	if (!bEjectPartInsideRoom4)
	{
		// 접속 종료(B): 방4 안에는 아무도 없으므로 부품을 방3 쪽 문 앞으로 옮긴다
		if (EjectedPart && Director->Room4EntranceDropPoint)
		{
			EjectedPart->SetActorLocation(
				Director->Room4EntranceDropPoint->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	// ④ D3 게이트 재활성. 진입 규칙은 부품 위치에 따라 달라진다.
	if (ADRS2TeleportGate* Gate = Director->Gate_Room3ToRoom4)
	{
		// ★부품이 방4 안에 있으면 "소지자만" 규칙을 유지하면 아무도 들어갈 수 없다.
		Gate->SetEntryRule(bEjectPartInsideRoom4 ? ES2GateEntryRule::Anyone : ES2GateEntryRule::CarrierOnly);
		Gate->SetGateActive(true);
	}

	Room4Player = nullptr;

	SetupPhaseObjectiveByRow(TEXT("S2P3_Enter4"));
}

void UDRS2DefensePhase::NotifyPlayerDied(APlayerState* DeadPlayerState)
{
	if (!bRoomStarted || bMoleGameCleared) return;
	if (!Room4Player.IsValid() || !DeadPlayerState) return;

	// 방4에 들어간 플레이어가 죽은 경우에만 처리한다
	if (Room4Player->GetPlayerState() != DeadPlayerState) return;

	ADRS2StageDirector* Director = GetDirector();
	if (!Director) return;

	UE_LOG(LogDR, Log, TEXT("[S2P3] 예외 A - 방4 플레이어 사망. 방3 정리 후 재시도 대기"));

	// 방3 몬스터 제거와 웨이브 중단은 PrepareRoom4Retry 가 공통 처리한다.
	// 부품은 방4 안(설치대 옆)에 남으므로 진입 규칙을 Anyone 으로 완화한다.
	PrepareRoom4Retry(Director, /*bEjectPartInsideRoom4=*/true);
}

void UDRS2DefensePhase::NotifyPlayerLeft(APlayerState* LeftPlayerState)
{
	if (!bRoomStarted || bMoleGameCleared) return;
	if (!Room4Player.IsValid() || !LeftPlayerState) return;

	if (Room4Player->GetPlayerState() != LeftPlayerState) return;

	ADRS2StageDirector* Director = GetDirector();
	if (!Director) return;

	UE_LOG(LogDR, Log, TEXT("[S2P3] 예외 B - 방4 플레이어 접속 종료. 방3 정리 후 재시도 대기"));

	// 부품을 방3 쪽 문 앞으로 되돌리고 원래 흐름(CarrierOnly)을 복원한다.
	// 방3 몬스터 제거와 웨이브 중단은 A 와 동일하게 PrepareRoom4Retry 가 처리한다.
	PrepareRoom4Retry(Director, /*bEjectPartInsideRoom4=*/false);

	// 영구 이탈이므로 기준 인원을 재계산한다 (Plan6 §14.3.6-7)
	ResolveBasePlayerCount();
}

// ================= 완료 / 정리 =================

bool UDRS2DefensePhase::IsCompleted() const
{
	return bMoleGameCleared;
}

void UDRS2DefensePhase::OnPhaseEnd()
{
	StopWaveLoop();

	if (ADRS2StageDirector* Director = GetDirector())
	{
		if (ADRS2RoomTrigger* Trigger = Director->Trigger_Room3)
		{
			Trigger->OnAllInside.RemoveDynamic(this, &UDRS2DefensePhase::HandleAllInsideRoom3);
			Trigger->OnCountChanged.RemoveDynamic(this, &UDRS2DefensePhase::HandleInsideCountChanged);
			Trigger->Disarm();
		}
		if (ADRS2TeleportGate* Gate = Director->Gate_Room3ToRoom4)
		{
			Gate->OnGateUsed.RemoveDynamic(this, &UDRS2DefensePhase::HandleRoom4Entered);
		}
		if (ADRCleanserSite* Site = Director->Room4InstallSite)
		{
			Site->OnPartInstalled.RemoveDynamic(this, &UDRS2DefensePhase::HandlePartInstalled);
		}
		if (ADRS2MoleGame* MoleGame = Director->Room4MoleGame)
		{
			MoleGame->OnProgress.RemoveDynamic(this, &UDRS2DefensePhase::HandleMoleProgress);
			MoleGame->OnCleared.RemoveDynamic(this, &UDRS2DefensePhase::HandleMoleGameCleared);
			MoleGame->AbortAndReset();
		}
	}

	Super::OnPhaseEnd();
}
