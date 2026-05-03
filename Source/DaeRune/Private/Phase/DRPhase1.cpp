// Copyright DaeRune


#include "Phase/DRPhase1.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Interaction/CombatInterface.h"
#include "Actor/DRCleanserSite.h"
#include "Actor/DRDoorManager.h"

void UDRPhase1::OnPhaseStart()
{
	Super::OnPhaseStart();

	if (!GameMode || !GameState) return;

	// 목표 설정
	SetupPhaseObjective(1);

	// GameState Phase 1 초기화
	GameState->SetCleanserAreaSecured(false);
	GameState->SetRemainingEnemiesInArea(0);

	// 클렌저 위치에 적 스폰
	SpawnEnemiesAtCleanserSites();

	FPhaseObjectiveData PhaseObjective = GameState->GetCurrentPhaseObjective();
	PhaseObjective.RequiredCount = TotalEnemyCount;
	GameState->SetPhaseObjective(PhaseObjective);

	// 총 적 수로 GameState 업데이트
	GameState->SetRemainingEnemiesInArea(TotalEnemyCount);

	GameState->SetInitialPlayerCount(GameState->GetAlivePlayers().Num());
}

void UDRPhase1::OnPhaseEnd()
{
	Super::OnPhaseEnd();

	// DoorManager에 알림
	if (ADRDoorManager* DoorMgr = GetDoorManager())
	{
		DoorMgr->OnPhase1Ended();
	}

	// Phase1 전용 정리
	SelectedCleanserSites.Empty();
	TotalEnemyCount = 0;
}

void UDRPhase1::OnEnemyDeath(AActor* DeadEnemy)
{
	Super::OnEnemyDeath(DeadEnemy);

	if (!GameMode || !GameState || !bIsPhaseActive) return;

	// 살아있는 적 수 계산
	int32 AliveCount = GetAliveEnemyCount();

	// GameState 업데이트
	GameState->SetRemainingEnemiesInArea(AliveCount);
	const int32 DefeatedEnemyCount = FMath::Max(0, TotalEnemyCount - AliveCount);
	GameState->UpdatePhaseObjectiveProgress(DefeatedEnemyCount);

	// 모든 적이 죽었으면 지역 확보 완료
	if (AliveCount == 0)
	{
		GameState->SetCleanserAreaSecured(true);
	}

	// GameMode에 완료 조건 체크 요청
	GameMode->ValidatePhaseCompletion();
}

void UDRPhase1::SpawnEnemiesAtCleanserSites()
{
	if (!GameMode) return;

	// PhaseBase에서 전체 클렌저 사이트 가져오기
	if (CleanserSites.Num() < 3) return;

	// 랜덤으로 제거할 1개 선택
	int32 IndexToRemove = FMath::RandRange(0, CleanserSites.Num() - 1);
	TObjectPtr<ADRCleanserSite> SiteToDestroy = CleanserSites[IndexToRemove];

	// 제거 대상 액터 파괴
	if (SiteToDestroy && IsValid(SiteToDestroy))
	{
		SiteToDestroy->Destroy();
	}

	// 배열에서 제거
	CleanserSites.RemoveAt(IndexToRemove);

	// 남은 2개를 활성 클렌저 사이트로 설정
	SelectedCleanserSites = CleanserSites; 
	SetActiveCleanserSites(SelectedCleanserSites);
	GameState->SetCleanserSites(SelectedCleanserSites);

	// 선택된 사이트 활성화 및 적 스폰
	for (const TObjectPtr<ADRCleanserSite>& SelectedSite : SelectedCleanserSites)
	{
		if (!SelectedSite) continue;

		// 클렌저 사이트 활성화
		SelectedSite->ActivateSite();

		// 해당 위치에 적 스폰
		SpawnEnemyGroupAtCleanserSite(SelectedSite.Get());
	}
}

void UDRPhase1::SpawnEnemyGroupAtCleanserSite(ADRCleanserSite* CleanserSite)
{
	if (!CleanserSite || !NormalEnemyClass) return;

	const TArray<FVector> SpawnLocations = CleanserSite->GetPhase1EnemySpawnLocations();
	if (SpawnLocations.Num() == 0) return;

	for (const FVector& SpawnLocation : SpawnLocations)
	{
		AActor* NormalEnemy = SpawnEnemy(NormalEnemyClass, SpawnLocation);
		if (NormalEnemy)
		{
			TotalEnemyCount++;
		}
	}
}

AActor* UDRPhase1::SpawnEnemy(TSubclassOf<AActor> EnemyClass, const FVector& Location)
{
	if (!GameMode || !EnemyClass) return nullptr;

	UWorld* World = GameMode->GetWorld();
	if (!World) return nullptr;

	// 스폰 파라미터 설정
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// 적 스폰
	AActor* SpawnedEnemy = World->SpawnActor<AActor>(EnemyClass, Location, FRotator::ZeroRotator, SpawnParams);

	if (SpawnedEnemy)
	{
		// 델리게이트 바인딩
		if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(SpawnedEnemy))
		{
			CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UDRPhase1::OnEnemyDeath);
		}

		// 스폰된 적 리스트에 추가
		SpawnedEnemies.Add(SpawnedEnemy);
	}

	return SpawnedEnemy;
}

ADRDoorManager* UDRPhase1::GetDoorManager()
{
	// 이미 캐싱되어 있으면 반환
	if (CachedDoorManager)
	{
		return CachedDoorManager;
	}

	// GameState에서 가져오기
	if (UWorld* World = GetWorld())
	{
		if (ADRStageGameState* StageGameState = World->GetGameState<ADRStageGameState>())
		{
			CachedDoorManager = StageGameState->GetDoorManager();
		}
	}

	return CachedDoorManager;
}
