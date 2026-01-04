// Copyright DaeRune


#include "Phase/DRPhase1.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Interaction/CombatInterface.h"
#include "Actor/DRCleanserSite.h"

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

	// 총 적 수로 GameState 업데이트
	GameState->SetRemainingEnemiesInArea(TotalEnemyCount);

	GameState->SetInitialPlayerCount(GameState->GetAlivePlayers().Num());
}

void UDRPhase1::OnPhaseEnd()
{
	Super::OnPhaseEnd();

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
	GameState->UpdatePhaseObjectiveProgress(GameState->CurrentPhaseObjective.RequiredCount - AliveCount);

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
	if (!CleanserSite || !EliteEnemyClass || !NormalEnemyClass) return;

	// 클렌저 사이트의 스폰 위치 가져오기
	FVector CenterLocation = CleanserSite->GetSpawnLocation();

	// 1. 엘리트 몬스터 1마리 (클렌저에서 약간 떨어진 위치)
	// 랜덤한 방향으로 EliteSpawnOffset만큼 떨어뜨리기
	float RandomAngle = FMath::FRandRange(0.0f, 360.0f);
	FVector EliteOffset = FVector(
		FMath::Cos(FMath::DegreesToRadians(RandomAngle)) * EliteSpawnOffset,
		FMath::Sin(FMath::DegreesToRadians(RandomAngle)) * EliteSpawnOffset,
		0.0f
	);
	FVector EliteSpawnLocation = CenterLocation + EliteOffset;

	AActor* EliteEnemy = SpawnEnemy(EliteEnemyClass, EliteSpawnLocation);
	if (EliteEnemy)
	{
		TotalEnemyCount++;
	}

	// 2. 일반 몬스터 4마리 (주변에 원형으로)
	for (int32 i = 0; i < NormalEnemyCount; i++)
	{
		// 원형 배치 계산
		float Angle = (360.0f / NormalEnemyCount) * i;
		FVector Offset = FVector(
			FMath::Cos(FMath::DegreesToRadians(Angle)) * SpawnRadius,
			FMath::Sin(FMath::DegreesToRadians(Angle)) * SpawnRadius,
			0.0f
		);

		FVector SpawnLocation = CenterLocation + Offset;

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
