// Copyright DaeRune

#include "Phase/Stage2/DRS2TrainPhase.h"

#include "AIController.h"
#include "Actor/Stage2/DRS2StageDirector.h"
#include "Actor/Stage2/DRS2Train.h"
#include "Actor/Stage2/DRS2TrainTrack.h"
#include "Actor/Stage2/DRS2TrainObstacle.h"
#include "Actor/Stage2/DRS2Barrier.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Character/DREnemy.h"
#include "Character/Stage2/DRS2MoleBoss.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "DaeRune/DRLogChannels.h"

void UDRS2TrainPhase::OnPhaseStart()
{
	Super::OnPhaseStart();

	if (!GameMode || !GameState) return;

	ADRS2StageDirector* Director = GetDirector();
	if (!Director) return;

	// ★기준 인원 확정 (Plan7 §5.1-①).
	//   두더지 보스의 융기 데미지와 전기장 데미지가 이 값을 공통 룩업 인덱스로 쓴다.
	//   방 시작 시점에 1회 확정하고 이후 사망해도 재계산하지 않는 것이 스테이지2 공통 규약이다.
	ResolveBasePlayerCount();

	// 기본 임계값 (BP 미설정 시)
	if (RetreatHealthRatios.Num() == 0)
	{
		RetreatHealthRatios = { 0.67f, 0.34f };
	}

	// D5 하강 개방 (멱등)
	SetBlockerBlocked(Director->Blocker_Room5ToRoom6, false);

	// 구간별 해제 래치 초기화
	bSegmentResolved.Init(false, Director->Obstacles.Num());
	NextObstacleIndex = 0;
	CurrentObstacleIndex = INDEX_NONE;

	// 장애물에 선로를 알려 StopDistance 자동 계산을 돕는다
	for (const TObjectPtr<ADRS2TrainObstacle>& Obstacle : Director->Obstacles)
	{
		if (Obstacle && Director->Track)
		{
			Obstacle->SetTrack(Director->Track);
		}
	}

	if (ADRS2Train* Train = Director->Train)
	{
		Train->OnBoardingChanged.AddDynamic(this, &UDRS2TrainPhase::HandleBoardingChanged);
		Train->OnStoppedAtTarget.AddDynamic(this, &UDRS2TrainPhase::HandleTrainStopped);
		Train->SetWaitingForBoarding();
	}
	else
	{
		UE_LOG(LogDR, Error, TEXT("[S2P5] Train 미배선 - 방6 진행 불가"));
	}

	SetupPhaseObjectiveByRow(TEXT("S2P5_Board"));
}

// ================= 탑승 / 출발 =================

void UDRS2TrainPhase::HandleBoardingChanged(int32 SeatedCount, int32 AliveTotal)
{
	if (!GameState) return;

	GameState->UpdatePhaseObjectiveProgress(SeatedCount);

	ADRS2StageDirector* Director = GetDirector();
	if (!Director || !Director->Train) return;

	// 생존자 전원 착석 시 출발 (빈 칸 허용 - §11.A-6)
	if (Director->Train->IsAcceptingBoarding() && Director->Train->AreAllAlivePlayersSeated())
	{
		DepartToNextTarget(Director);
	}
}

void UDRS2TrainPhase::DepartToNextTarget(ADRS2StageDirector* Director)
{
	if (!Director || !Director->Train || !Director->Track) return;

	// 직전 구간의 후방 배리어를 해제한다 (열차 출발과 함께)
	const int32 PreviousIndex = CurrentObstacleIndex;
	if (Director->RearBarriers.IsValidIndex(PreviousIndex) && Director->RearBarriers[PreviousIndex])
	{
		Director->RearBarriers[PreviousIndex]->SetBarrierEnabled(false);
	}

	float TargetDistance = 0.f;
	int32 TargetIndex = INDEX_NONE;

	if (Director->Obstacles.IsValidIndex(NextObstacleIndex))
	{
		const ADRS2TrainObstacle* Obstacle = Director->Obstacles[NextObstacleIndex];
		if (!Obstacle) return;

		TargetDistance = Obstacle->GetStopDistance();
		TargetIndex = NextObstacleIndex;
	}
	else
	{
		// 남은 장애물이 없으면 종점으로 (실제로는 마지막 보스 처치로 클리어되므로 도달하지 않는다)
		TargetDistance = Director->Track->GetTrackLength();
		TargetIndex = INDEX_NONE;
	}

	Director->Train->DepartTo(TargetDistance, TrainSpeed, TargetIndex);

	SetupPhaseObjectiveByRow(TEXT("S2P5_Ride"));
}

// ================= 장애물 도착 / 보스 등장 =================

void UDRS2TrainPhase::HandleTrainStopped(int32 ObstacleIndex)
{
	ADRS2StageDirector* Director = GetDirector();
	if (!Director || !GameState) return;

	if (!Director->Obstacles.IsValidIndex(ObstacleIndex)) return;

	CurrentObstacleIndex = ObstacleIndex;
	NextObstacleIndex = ObstacleIndex + 1;

	ADRS2TrainObstacle* Obstacle = Director->Obstacles[ObstacleIndex];
	if (!Obstacle) return;

	// ★보스 등장과 동시에 장애물이 부서진다 (§14.6.3)
	Obstacle->BreakByBoss();

	// 장애물이 부서지므로 전방·후방 모두 배리어로 전투 구간을 한정한다 (§14.6.5)
	if (Director->ForwardBarriers.IsValidIndex(ObstacleIndex) && Director->ForwardBarriers[ObstacleIndex])
	{
		Director->ForwardBarriers[ObstacleIndex]->SetBarrierEnabled(true);
	}
	if (Director->RearBarriers.IsValidIndex(ObstacleIndex) && Director->RearBarriers[ObstacleIndex])
	{
		Director->RearBarriers[ObstacleIndex]->SetBarrierEnabled(true);
	}

	const FTransform SpawnTransform = Obstacle->GetBossSpawnTransform();

	if (!MoleBoss.IsValid())
	{
		// 첫 구간: 보스를 새로 스폰한다.
		// ★레벨에 기준 인원을 심는다 — 이 값이 CT_EnemyAttributes(Mole.MaxHealth)와
		//   어빌리티 스펙 레벨을 동시에 인덱싱해 "인원 축" 스케일링 전체를 만든다.
		//   반드시 BeginPlay 전에 들어가야 하므로 지연 스폰 경로를 쓴다 (Plan7 §5.6).
		MoleBoss = Cast<ADREnemy>(SpawnEnemyAt(MoleBossClass, SpawnTransform, BasePlayerCount));
		if (!MoleBoss.IsValid())
		{
			UE_LOG(LogDR, Error, TEXT("[S2P5] 보스 스폰 실패 - MoleBossClass 를 확인하세요."));
			return;
		}
	}
	else
	{
		// ★2·3 구간: 같은 개체를 되살린다 (체력이 이어진다)
		ReappearBoss(SpawnTransform);
	}

	// ★두더지 보스 주입 (Plan7 §5.1-②). 임시 EliteBear 등 다른 클래스면 Cast 가 실패해 조용히 건너뛴다.
	// 인원 축(체력·융기 데미지·전기장 데미지)은 스폰 시 심은 Level 에서 파생되므로 여기서 넣을 것이 없다.
	if (ADRS2MoleBoss* MoleBossActor = Cast<ADRS2MoleBoss>(MoleBoss.Get()))
	{
		MoleBossActor->SetSegmentIndex(ObstacleIndex);   // 구간 축: 발톱 데미지 · 스킬 쿨다운 · 전기장 활성
		MoleBossActor->NotifyReappeared();               // 이동 봉인 재적용 + 반매몰 자세 정렬 (첫 스폰에도 안전)
	}

	BindBossHealth();

	SetupPhaseObjectiveByRow(TEXT("S2P5_Boss"), Director->Obstacles.Num());
	GameState->UpdatePhaseObjectiveProgress(ObstacleIndex + 1);

	UE_LOG(LogDR, Log, TEXT("[S2P5] 구간 %d 전투 시작"), ObstacleIndex + 1);
}

// ================= 보스 체력 추적 =================

void UDRS2TrainPhase::BindBossHealth()
{
	if (!MoleBoss.IsValid()) return;

	CachedBossMaxHealth = 0.f;

	MoleBoss->OnHealthChanged.AddDynamic(this, &UDRS2TrainPhase::HandleBossHealthChanged);
	MoleBoss->OnMaxHealthChanged.AddDynamic(this, &UDRS2TrainPhase::HandleBossMaxHealthChanged);
}

void UDRS2TrainPhase::UnbindBossHealth()
{
	if (!MoleBoss.IsValid()) return;

	MoleBoss->OnHealthChanged.RemoveDynamic(this, &UDRS2TrainPhase::HandleBossHealthChanged);
	MoleBoss->OnMaxHealthChanged.RemoveDynamic(this, &UDRS2TrainPhase::HandleBossMaxHealthChanged);
}

void UDRS2TrainPhase::HandleBossMaxHealthChanged(float NewValue)
{
	CachedBossMaxHealth = NewValue;
}

void UDRS2TrainPhase::HandleBossHealthChanged(float NewValue)
{
	const int32 Index = CurrentObstacleIndex;

	// ★마지막 구간에는 임계가 없다. 사망만이 해제 조건이다.
	if (!RetreatHealthRatios.IsValidIndex(Index)) return;
	if (bSegmentResolved.IsValidIndex(Index) && bSegmentResolved[Index]) return;
	if (CachedBossMaxHealth <= 0.f) return;

	const float Ratio = NewValue / CachedBossMaxHealth;
	if (Ratio <= RetreatHealthRatios[Index])
	{
		UE_LOG(LogDR, Log, TEXT("[S2P5] 구간 %d 임계 도달 (%.0f%%) - 보스 도망"), Index + 1, Ratio * 100.f);
		ResolveSegment(Index);
	}
}

// ================= 구간 해제 =================

void UDRS2TrainPhase::ResolveSegment(int32 ObstacleIndex)
{
	if (!bSegmentResolved.IsValidIndex(ObstacleIndex) || bSegmentResolved[ObstacleIndex]) return;

	bSegmentResolved[ObstacleIndex] = true;

	ADRS2StageDirector* Director = GetDirector();
	if (!Director) return;

	// ① 바인딩을 먼저 끊는다 (보관 중 중복 발화 방지)
	UnbindBossHealth();

	// ② 도망 = 비활성 보관 (Destroy 금지 - 체력을 유지해야 한다)
	StashBoss();

	// ③ 전방 배리어 해제 (열차가 통과할 수 있게)
	if (Director->ForwardBarriers.IsValidIndex(ObstacleIndex) && Director->ForwardBarriers[ObstacleIndex])
	{
		Director->ForwardBarriers[ObstacleIndex]->SetBarrierEnabled(false);
	}

	// ④ 재탑승 목표. 전원 재탑승 시 HandleBoardingChanged 경로로 재출발한다.
	if (Director->Train)
	{
		Director->Train->SetWaitingForBoarding();
	}
	SetupPhaseObjectiveByRow(TEXT("S2P5_Board"));
}

void UDRS2TrainPhase::StashBoss()
{
	if (!MoleBoss.IsValid()) return;

	ADREnemy* Boss = MoleBoss.Get();

	// ★숨기기 전에 반드시 통지한다 (Plan7 §1.2 계약 C5).
	//   진행 중이던 굴착 강습을 취소하지 않으면 GA 의 타이머 체인이 살아남아
	//   "보관 중인(숨겨진) 보스가 융기해 데미지를 주는" 버그가 난다.
	if (ADRS2MoleBoss* MoleBossActor = Cast<ADRS2MoleBoss>(Boss))
	{
		MoleBossActor->NotifyStashed();
	}

	Boss->SetActorHiddenInGame(true);
	Boss->SetActorEnableCollision(false);
	Boss->SetActorTickEnabled(false);

	if (AAIController* AIController = Cast<AAIController>(Boss->GetController()))
	{
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("S2 Boss Retreat"));
		}
	}
}

void UDRS2TrainPhase::ReappearBoss(const FTransform& SpawnTransform)
{
	if (!MoleBoss.IsValid()) return;

	ADREnemy* Boss = MoleBoss.Get();

	Boss->SetActorTransform(SpawnTransform);
	Boss->SetActorHiddenInGame(false);
	Boss->SetActorEnableCollision(true);
	Boss->SetActorTickEnabled(true);

	if (AAIController* AIController = Cast<AAIController>(Boss->GetController()))
	{
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->RestartLogic();
		}
	}

	// ★체력은 손대지 않는다. 67% -> 34% -> 0% 로 이어지는 것이 사양이다.
}

// ================= 사망 / 완료 =================

void UDRS2TrainPhase::OnEnemyDeath(AActor* DeadEnemy)
{
	Super::OnEnemyDeath(DeadEnemy);

	if (!MoleBoss.IsValid() || DeadEnemy != MoleBoss.Get()) return;

	ADRS2StageDirector* Director = GetDirector();
	const int32 LastIndex = Director ? Director->Obstacles.Num() - 1 : INDEX_NONE;

	if (CurrentObstacleIndex == LastIndex)
	{
		// ★최종 처치 = 스테이지2 클리어 (도착지점 이동 없음 - §14.6.6)
		UnbindBossHealth();
		bBossDefeated = true;

		UE_LOG(LogDR, Log, TEXT("[S2P5] 보스 최종 처치 - 스테이지2 클리어"));

		if (GameMode)
		{
			GameMode->ValidatePhaseCompletion();
		}
	}
	else
	{
		// 임계를 지나쳐 즉사시킨 예외 케이스도 진행으로 인정한다.
		// 단 보스가 죽었으므로 다음 구간에 재등장시킬 수 없다 -> 새로 스폰되도록 참조를 비운다.
		UE_LOG(LogDR, Warning,
			TEXT("[S2P5] 구간 %d 에서 보스가 즉사했습니다. 다음 구간에서는 새 보스가 스폰됩니다."),
			CurrentObstacleIndex + 1);

		UnbindBossHealth();
		MoleBoss = nullptr;
		ResolveSegment(CurrentObstacleIndex);
	}
}

bool UDRS2TrainPhase::IsCompleted() const
{
	return bBossDefeated;
}

void UDRS2TrainPhase::OnPhaseEnd()
{
	UnbindBossHealth();

	// ★남아 있는 전기장을 회수한다 (Plan7 §5.1-④). 보스가 파괴되기 전에 먼저 정리해야
	//   플레이어에게 걸린 주기 데미지 GE 까지 확실히 제거된다.
	if (ADRS2MoleBoss* MoleBossActor = Cast<ADRS2MoleBoss>(MoleBoss.Get()))
	{
		MoleBossActor->NotifyStashed();
	}

	if (ADRS2StageDirector* Director = GetDirector())
	{
		if (ADRS2Train* Train = Director->Train)
		{
			Train->OnBoardingChanged.RemoveDynamic(this, &UDRS2TrainPhase::HandleBoardingChanged);
			Train->OnStoppedAtTarget.RemoveDynamic(this, &UDRS2TrainPhase::HandleTrainStopped);
			Train->DeboardAll();
		}

		// 배리어 전부 해제
		for (const TObjectPtr<ADRS2Barrier>& Barrier : Director->ForwardBarriers)
		{
			if (Barrier) Barrier->SetBarrierEnabled(false);
		}
		for (const TObjectPtr<ADRS2Barrier>& Barrier : Director->RearBarriers)
		{
			if (Barrier) Barrier->SetBarrierEnabled(false);
		}
	}

	// 보관 중이던 보스가 남아 있으면 Super 의 SpawnedEnemies 정리에서 파괴된다.
	Super::OnPhaseEnd();
}
