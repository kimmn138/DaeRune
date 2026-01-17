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

	// 목표 설정
	SetupPhaseObjective(2);

	// GameState Phase2 초기화
	GameState->SetCollectedParts(0);
	GameState->SetCleanserActivated(false);

	// Phase1에서 선택된 활성 클렌저 사이트 가져오기
	const TArray<TObjectPtr<ADRCleanserSite>>& ActiveSites = GetActiveCleanserSites();

	// 클렌저 사이트 유효성 검증
	if (ActiveSites.Num() != 2) return;

	// 완료된 사이트 추적 초기화
	CompletedSites.Empty();

	// 각 클렌저 사이트의 부품 설치 델리게이트 구독
	for (ADRCleanserSite* Site : ActiveSites)
	{
		if (Site)
		{
			// 부품 설치 이벤트 구독
			Site->OnPartInstalled.AddDynamic(this, &UDRPhase2::OnPartInstalled);
		}
	}

	// 스폰 포인트 찾기
	FindEnemySpawnPoints();

	// 부품을 들고 도망치는 적 스폰
	SpawnPartCarryingEnemies();
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
	if (!GameMode) return;

	UWorld* World = GameMode->GetWorld();
	if (!World) return;

	EnemySpawnPoints.Empty();

	// 태그로 레벨에서 스폰 포인트 찾기
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

	// 적 클래스 유효성 검증
	if (!PartCarryingEnemyClass) return;

	// 스폰 포인트 유효성 검증
	if (EnemySpawnPoints.Num() != 4) return;

	// 각 스폰 포인트에 적 스폰
	for (AActor* SpawnPoint : EnemySpawnPoints)
	{
		if (!SpawnPoint) continue;

		ADREnemy* SpawnedEnemy = SpawnEnemyAtLocation(SpawnPoint);
		if (SpawnedEnemy)
		{
			// 델리게이트 바인딩
			if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(SpawnedEnemy))
			{
				CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UDRPhaseBase::OnEnemyDeath);
			}
			
			// 스폰된 적 추적 (OnPhaseEnd에서 남은 적 정리용)
			SpawnedEnemies.Add(SpawnedEnemy);
		}
	}
}

ADREnemy* UDRPhase2::SpawnEnemyAtLocation(AActor* SpawnPoint)
{
	if (!GameMode || !SpawnPoint) return nullptr;

	UWorld* World = GameMode->GetWorld();
	if (!World) return nullptr;

	// 스폰 포인트의 위치와 회전 가져오기
	FVector Location = SpawnPoint->GetActorLocation();
	FRotator Rotation = SpawnPoint->GetActorRotation();

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
	GameState->UpdatePhaseObjectiveProgress(TotalInstalledParts);

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
