// Copyright DaeRune


#include "Phase/DRPhase2.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Actor/DRCleanserSite.h"
#include "Kismet/GameplayStatics.h"
#include "Character/DREnemy.h"
#include "EngineUtils.h"

void UDRPhase2::OnPhaseStart()
{
	Super::OnPhaseStart();

	if (!GameMode || !GameState) return;

	// 占쏙옙표 占쏙옙占쏙옙
	SetupPhaseObjective(2);

	// GameState Phase2 占십깍옙화
	GameState->SetCollectedParts(0);
	GameState->SetCleanserActivated(false);

	// Phase1?먯꽌 ?좏깮???쒖꽦 ?대젋? ?ъ씠??媛?몄삤湲?
	const TArray<TObjectPtr<ADRCleanserSite>>& ActiveSites = GetActiveCleanserSites();

	// ?대젋? ?ъ씠???좏슚??寃??
	if (ActiveSites.Num() != 2)
	{
return;
	}

	// 占싹뤄옙占?占쏙옙占쏙옙트 占쏙옙占쏙옙 占십깍옙화
	CompletedSites.Empty();

	// 占쏙옙 클占쏙옙占쏙옙 占쏙옙占쏙옙트占쏙옙 占쏙옙품 占쏙옙치 占쏙옙占쏙옙占쏙옙占쏙옙트 占쏙옙占쏙옙
	for (ADRCleanserSite* Site : ActiveSites)
	{
		if (Site)
		{
			// 占쏙옙품 占쏙옙치 占싱븝옙트 占쏙옙占쏙옙
			Site->OnPartInstalled.AddDynamic(this, &UDRPhase2::OnPartInstalled);
		}
	}

	// 占쏙옙占쏙옙 占쏙옙占쏙옙트 찾占쏙옙
	FindEnemySpawnPoints();

	// 占쏙옙품占쏙옙 占쏙옙占?占쏙옙占쏙옙치占쏙옙 占쏙옙 占쏙옙占쏙옙
	SpawnPartCarryingEnemies();
}

void UDRPhase2::OnPhaseEnd()
{
	// 클占쏙옙占쏙옙 占쏙옙占쏙옙트 占쏙옙占쏙옙占쏙옙占쏙옙트 占쏙옙占쏙옙 占쏙옙占쏙옙
	const TArray<TObjectPtr<ADRCleanserSite>>& ActiveSites = GetActiveCleanserSites();
	for (ADRCleanserSite* Site : ActiveSites)
	{
		if (Site)
		{
			Site->OnPartInstalled.RemoveDynamic(this, &UDRPhase2::OnPartInstalled);
		}
	}

	// 占싹뤄옙 占쏙옙占쏙옙 占십깍옙화
	CompletedSites.Empty();

	// 占쏙옙占쏙옙 占쏙옙占쏙옙트 占십깍옙화
	EnemySpawnPoints.Empty();

	Super::OnPhaseEnd();
}

void UDRPhase2::FindEnemySpawnPoints()
{
	if (!GameMode) return;

	UWorld* World = GameMode->GetWorld();
	if (!World) return;

	EnemySpawnPoints.Empty();

	// 占승그뤄옙 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙트 찾占쏙옙
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->ActorHasTag(SpawnPointTag))
		{
			EnemySpawnPoints.Add(Actor);
		}
	}
}

void UDRPhase2::SpawnPartCarryingEnemies()
{
	if (!GameMode) return;

	// ???대옒???좏슚??寃??
	if (!PartCarryingEnemyClass)
	{
return;
	}

	// ?ㅽ룿 ?ъ씤???좏슚??寃??
	if (EnemySpawnPoints.Num() < 1)
	{
return;
	}

	if (EnemySpawnPoints.Num() != 4)
	{
}

	// 媛??ㅽ룿 ?ъ씤?몄뿉 ???앹꽦
	for (AActor* SpawnPoint : EnemySpawnPoints)
	{
		if (!SpawnPoint) continue;

		ADREnemy* SpawnedEnemy = SpawnEnemyAtLocation(SpawnPoint);
		if (SpawnedEnemy)
		{
			// 占쏙옙占쏙옙占쏙옙占쏙옙트 占쏙옙占싸듸옙
			if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(SpawnedEnemy))
			{
				CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UDRPhaseBase::OnEnemyDeath);
			}
			
			// 占쏙옙占쏙옙占쏙옙 占쏙옙 占쏙옙占쏙옙 (OnPhaseEnd占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙 占쏙옙占쏙옙占쏙옙)
			SpawnedEnemies.Add(SpawnedEnemy);
		}
	}
}

ADREnemy* UDRPhase2::SpawnEnemyAtLocation(AActor* SpawnPoint)
{
	if (!GameMode || !SpawnPoint) return nullptr;

	UWorld* World = GameMode->GetWorld();
	if (!World) return nullptr;

	// 占쏙옙占쏙옙 占쏙옙占쏙옙트占쏙옙 占쏙옙치占쏙옙 회占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙
	FVector Location = SpawnPoint->GetActorLocation();
	FRotator Rotation = SpawnPoint->GetActorRotation();

	// 占쏙옙占쏙옙 占식띰옙占쏙옙占?占쏙옙占쏙옙
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// 占쏙옙 占쏙옙占쏙옙
	ADREnemy* SpawnedEnemy = World->SpawnActor<ADREnemy>(
		PartCarryingEnemyClass,
		Location,
		Rotation,
		SpawnParams
	);

	return SpawnedEnemy;
}

void UDRPhase2::OnPartInstalled(ADRCleanserSite* Site)
{
	if (!Site || !GameState) return;

	// GameState 占쏙옙占쏙옙占쏙옙트: 占쏙옙치占쏙옙 占쏙옙품 占쏙옙占쏙옙 占쏙옙占쏙옙
	int32 TotalInstalledParts = 0;
	const TArray<TObjectPtr<ADRCleanserSite>>& ActiveSites = GetActiveCleanserSites();
	for (ADRCleanserSite* ActiveSite : ActiveSites)
	{
		if (ActiveSite)
		{
			TotalInstalledParts += ActiveSite->GetInstalledPartsCount();
		}
	}
	GameState->SetCollectedParts(TotalInstalledParts);
	GameState->UpdatePhaseObjectiveProgress(TotalInstalledParts);

	// 占쌔댐옙 占쏙옙占쏙옙트占쏙옙 占쏙옙품 占쏙옙치占쏙옙 占싹뤄옙퓸占쏙옙占쏙옙占?확占쏙옙
	if (Site->IsPartInstallationComplete())
	{
		// 占싹뤄옙占?占쏙옙占쏙옙트占쏙옙 占쏙옙占쏙옙
		CompletedSites.Add(Site);
	}

	// 占쏙옙占쏙옙占쏙옙 占싹뤄옙 占쏙옙占쏙옙 체크
	CheckPhaseCompletion();
}

void UDRPhase2::CheckPhaseCompletion()
{
	// 占쏙옙占?클占쏙옙占쏙옙 占쏙옙占쏙옙트占쏙옙 占쏙옙품占쏙옙 2占쏙옙占쏙옙 占쏙옙치占실억옙占쏙옙占쏙옙 확占쏙옙
	const TArray<TObjectPtr<ADRCleanserSite>>& ActiveSites = GetActiveCleanserSites();

	// 활占쏙옙 占쏙옙占쏙옙트占쏙옙 2占쏙옙占쏙옙占쏙옙 확占쏙옙
	if (ActiveSites.Num() != 2) return;

	// 占싹뤄옙占?占쏙옙占쏙옙트占쏙옙 2占쏙옙占쏙옙占쏙옙 확占쏙옙
	if (CompletedSites.Num() == 2)
	{
		// 클占쏙옙占쏙옙 활占쏙옙화 占쏙옙占승뤄옙 占쏙옙占쏙옙
		GameState->SetCleanserActivated(true);

		// Phase2 占싹뤄옙
		if (GameMode)
		{
			GameMode->ValidatePhaseCompletion();
		}
	}
}

