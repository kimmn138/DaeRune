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

	UE_LOG(LogTemp, Warning, TEXT("========== Phase2: OnPhaseStart =========="));

	if (!GameMode || !GameState)
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: GameMode or GameState is null!"));  // ← 추가
		return;
	}

	// GameState Phase2 초기화
	GameState->SetCollectedParts(0);
	GameState->SetCleanserActivated(false);
	UE_LOG(LogTemp, Log, TEXT("Phase2: GameState initialized"));  // ← 추가

	// Phase1에서 선택된 활성 클렌저 사이트 가져오기
	const TArray<TObjectPtr<ADRCleanserSite>>& ActiveSites = GetActiveCleanserSites();

	UE_LOG(LogTemp, Warning, TEXT("Phase2: Found %d active cleanser sites"), ActiveSites.Num());  // ← 추가

	// 클렌저 사이트 유효성 검증
	if (ActiveSites.Num() != 2)
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: ActiveCleanserSites must be exactly 2!"));
		return;
	}

	// 완료된 사이트 추적 초기화
	CompletedSites.Empty();

	// 각 클렌저 사이트의 부품 설치 델리게이트 구독
	for (ADRCleanserSite* Site : ActiveSites)
	{
		if (Site)
		{
			// 부품 설치 이벤트 구독
			Site->OnPartInstalled.AddDynamic(this, &UDRPhase2::OnPartInstalled);
			UE_LOG(LogTemp, Log, TEXT("Phase2: Subscribed to OnPartInstalled for site %s"), *Site->GetName());  // ← 추가
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Phase2: Finding spawn points..."));  // ← 추가

	// 스폰 포인트 찾기
	FindEnemySpawnPoints();

	UE_LOG(LogTemp, Warning, TEXT("Phase2: Spawning enemies..."));  // ← 추가

	// 부품을 들고 도망치는 적 스폰
	SpawnPartCarryingEnemies();

	UE_LOG(LogTemp, Warning, TEXT("Phase2: OnPhaseStart completed"));  // ← 추가
}

void UDRPhase2::OnPhaseEnd()
{
	// 클렌저 사이트 델리게이트 구독 해제
	const TArray<TObjectPtr<ADRCleanserSite>>& ActiveSites = GetActiveCleanserSites();
	for (ADRCleanserSite* Site : ActiveSites)
	{
		if (Site)
		{
			Site->OnPartInstalled.RemoveDynamic(this, &UDRPhase2::OnPartInstalled);
		}
	}

	// 완료 추적 초기화
	CompletedSites.Empty();

	// 스폰 포인트 초기화
	EnemySpawnPoints.Empty();

	Super::OnPhaseEnd();
}

void UDRPhase2::FindEnemySpawnPoints()
{
	if (!GameMode)
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: GameMode is null in FindEnemySpawnPoints!"));  // ← 추가
		return;
	}

	UWorld* World = GameMode->GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: World is null!"));  // ← 추가
		return;
	}

	EnemySpawnPoints.Empty();

	UE_LOG(LogTemp, Warning, TEXT("Phase2: Searching for spawn points with tag '%s'"), *SpawnPointTag.ToString());  // ← 추가

	// 태그로 레벨에서 스폰 포인트 찾기
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->ActorHasTag(SpawnPointTag))
		{
			EnemySpawnPoints.Add(Actor);
			UE_LOG(LogTemp, Log, TEXT("Phase2: Found spawn point: %s at %s"),
				*Actor->GetName(), *Actor->GetActorLocation().ToString());  // ← 추가
		}
	}

	// 유효성 검증
	if (EnemySpawnPoints.Num() != 4)
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: Found %d spawn points with tag '%s', but need exactly 4!"),
			EnemySpawnPoints.Num(), *SpawnPointTag.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Phase2: Found 4 spawn points successfully"));
	}
}

void UDRPhase2::SpawnPartCarryingEnemies()
{
	if (!GameMode)
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: GameMode is null in SpawnPartCarryingEnemies!"));  // ← 추가
		return;
	}

	// 적 클래스 유효성 검증
	if (!PartCarryingEnemyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: PartCarryingEnemyClass is not set!"));
		return;
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Phase2: PartCarryingEnemyClass is set to %s"),
			*PartCarryingEnemyClass->GetName());  // ← 추가
	}

	// 스폰 포인트 유효성 검증
	if (EnemySpawnPoints.Num() != 4)
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: Cannot spawn enemies, spawn points count is %d (need 4)!"),
			EnemySpawnPoints.Num());  // ← 수정
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Phase2: Starting enemy spawn loop..."));  // ← 추가

	// 각 스폰 포인트에 적 스폰
	int32 SpawnIndex = 0;  // ← 추가
	for (AActor* SpawnPoint : EnemySpawnPoints)
	{
		SpawnIndex++;  // ← 추가
		if (!SpawnPoint)
		{
			UE_LOG(LogTemp, Warning, TEXT("Phase2: Spawn point %d is null, skipping..."), SpawnIndex);
			continue;
		}

		UE_LOG(LogTemp, Warning, TEXT("Phase2: Spawning enemy %d at %s"),
			SpawnIndex, *SpawnPoint->GetName());  // ← 추가

		ADREnemy* SpawnedEnemy = SpawnEnemyAtLocation(SpawnPoint);
		if (SpawnedEnemy)
		{
			// 스폰된 적 추적 (OnPhaseEnd에서 남은 적 정리용)
			SpawnedEnemies.Add(SpawnedEnemy);
			UE_LOG(LogTemp, Warning, TEXT("Phase2: Successfully spawned enemy %d. Total spawned: %d"),
				SpawnIndex, SpawnedEnemies.Num());  // ← 추가
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Phase2: Failed to spawn enemy %d!"), SpawnIndex);  // ← 추가
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Phase2: Spawn loop completed. Total enemies spawned: %d"),
		SpawnedEnemies.Num());  // ← 추가
}

ADREnemy* UDRPhase2::SpawnEnemyAtLocation(AActor* SpawnPoint)
{
	if (!GameMode || !SpawnPoint)
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: GameMode or SpawnPoint is null in SpawnEnemyAtActor!"));  // ← 추가
		return nullptr;
	}

	UWorld* World = GameMode->GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: World is null in SpawnEnemyAtActor!"));  // ← 추가
		return nullptr;
	}

	// 스폰 포인트의 위치와 회전 가져오기
	FVector Location = SpawnPoint->GetActorLocation();
	FRotator Rotation = SpawnPoint->GetActorRotation();

	UE_LOG(LogTemp, Log, TEXT("Phase2: Attempting to spawn %s at location %s"),
		*PartCarryingEnemyClass->GetName(), *Location.ToString());  // ← 추가

	// 스폰 파라미터 설정
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// 적 스폰
	ADREnemy* SpawnedEnemy = World->SpawnActor<ADREnemy>(
		PartCarryingEnemyClass,
		Location,
		Rotation,
		SpawnParams
	);

	if (SpawnedEnemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("Phase2: Spawned part-carrying enemy '%s' at %s (from %s)"),
			*SpawnedEnemy->GetName(), *Location.ToString(), *SpawnPoint->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: Failed to spawn enemy at %s (from %s)"),
			*Location.ToString(), *SpawnPoint->GetName());
	}

	return SpawnedEnemy;
}

void UDRPhase2::OnPartInstalled(ADRCleanserSite* Site)
{
	if (!Site || !GameState) return;

	// GameState 업데이트: 설치된 부품 개수 증가
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

	// 해당 사이트의 부품 설치가 완료되었는지 확인
	if (Site->IsPartInstallationComplete())
	{
		// 완료된 사이트로 추적
		CompletedSites.Add(Site);
	}

	// 페이즈 완료 조건 체크
	CheckPhaseCompletion();
}

void UDRPhase2::CheckPhaseCompletion()
{
	// 모든 클렌저 사이트에 부품이 2개씩 설치되었는지 확인
	const TArray<TObjectPtr<ADRCleanserSite>>& ActiveSites = GetActiveCleanserSites();

	// 활성 사이트가 2개인지 확인
	if (ActiveSites.Num() != 2) return;

	// 완료된 사이트가 2개인지 확인
	if (CompletedSites.Num() == 2)
	{
		// 클렌저 활성화 상태로 변경
		GameState->SetCleanserActivated(true);

		// Phase2 완료
		if (GameMode)
		{
			GameMode->ValidatePhaseCompletion();
		}
	}
}
