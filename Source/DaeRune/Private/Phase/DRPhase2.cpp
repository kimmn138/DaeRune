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

	// ��ǥ ����
	SetupPhaseObjective(2);

	// GameState Phase2 �ʱ�ȭ
	GameState->SetCollectedParts(0);
	GameState->SetCleanserActivated(false);

	// Phase1에서 선택된 활성 클렌저 사이트 가져오기
	const TArray<TObjectPtr<ADRCleanserSite>>& ActiveSites = GetActiveCleanserSites();

	// 클렌저 사이트 유효성 검사
	if (ActiveSites.Num() != 2)
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: ActiveCleanserSites가 2개가 아닙니다! 현재: %d개"), ActiveSites.Num());
		return;
	}

	// �Ϸ�� ����Ʈ ���� �ʱ�ȭ
	CompletedSites.Empty();

	// �� Ŭ���� ����Ʈ�� ��ǰ ��ġ ��������Ʈ ����
	for (ADRCleanserSite* Site : ActiveSites)
	{
		if (Site)
		{
			// ��ǰ ��ġ �̺�Ʈ ����
			Site->OnPartInstalled.AddDynamic(this, &UDRPhase2::OnPartInstalled);
		}
	}

	// ���� ����Ʈ ã��
	FindEnemySpawnPoints();

	// ��ǰ�� ��� ����ġ�� �� ����
	SpawnPartCarryingEnemies();
}

void UDRPhase2::OnPhaseEnd()
{
	// Ŭ���� ����Ʈ ��������Ʈ ���� ����
	const TArray<TObjectPtr<ADRCleanserSite>>& ActiveSites = GetActiveCleanserSites();
	for (ADRCleanserSite* Site : ActiveSites)
	{
		if (Site)
		{
			Site->OnPartInstalled.RemoveDynamic(this, &UDRPhase2::OnPartInstalled);
		}
	}

	// �Ϸ� ���� �ʱ�ȭ
	CompletedSites.Empty();

	// ���� ����Ʈ �ʱ�ȭ
	EnemySpawnPoints.Empty();

	Super::OnPhaseEnd();
}

void UDRPhase2::FindEnemySpawnPoints()
{
	if (!GameMode) return;

	UWorld* World = GameMode->GetWorld();
	if (!World) return;

	EnemySpawnPoints.Empty();

	// �±׷� �������� ���� ����Ʈ ã��
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

	// 적 클래스 유효성 검사
	if (!PartCarryingEnemyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: PartCarryingEnemyClass가 설정되지 않았습니다!"));
		return;
	}

	// 스폰 포인트 유효성 검사
	if (EnemySpawnPoints.Num() < 1)
	{
		UE_LOG(LogTemp, Error, TEXT("Phase2: 스폰 포인트를 찾을 수 없습니다! SpawnPointTag: %s"), *SpawnPointTag.ToString());
		return;
	}

	if (EnemySpawnPoints.Num() != 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("Phase2: 스폰 포인트가 4개가 아닙니다. 현재: %d개"), EnemySpawnPoints.Num());
	}

	// 각 스폰 포인트에 적 생성
	for (AActor* SpawnPoint : EnemySpawnPoints)
	{
		if (!SpawnPoint) continue;

		ADREnemy* SpawnedEnemy = SpawnEnemyAtLocation(SpawnPoint);
		if (SpawnedEnemy)
		{
			// ��������Ʈ ���ε�
			if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(SpawnedEnemy))
			{
				CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UDRPhaseBase::OnEnemyDeath);
			}
			
			// ������ �� ���� (OnPhaseEnd���� ���� �� ������)
			SpawnedEnemies.Add(SpawnedEnemy);
		}
	}
}

ADREnemy* UDRPhase2::SpawnEnemyAtLocation(AActor* SpawnPoint)
{
	if (!GameMode || !SpawnPoint) return nullptr;

	UWorld* World = GameMode->GetWorld();
	if (!World) return nullptr;

	// ���� ����Ʈ�� ��ġ�� ȸ�� ��������
	FVector Location = SpawnPoint->GetActorLocation();
	FRotator Rotation = SpawnPoint->GetActorRotation();

	// ���� �Ķ���� ����
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// �� ����
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

	// GameState ������Ʈ: ��ġ�� ��ǰ ���� ����
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

	// �ش� ����Ʈ�� ��ǰ ��ġ�� �Ϸ�Ǿ����� Ȯ��
	if (Site->IsPartInstallationComplete())
	{
		// �Ϸ�� ����Ʈ�� ����
		CompletedSites.Add(Site);
	}

	// ������ �Ϸ� ���� üũ
	CheckPhaseCompletion();
}

void UDRPhase2::CheckPhaseCompletion()
{
	// ��� Ŭ���� ����Ʈ�� ��ǰ�� 2���� ��ġ�Ǿ����� Ȯ��
	const TArray<TObjectPtr<ADRCleanserSite>>& ActiveSites = GetActiveCleanserSites();

	// Ȱ�� ����Ʈ�� 2������ Ȯ��
	if (ActiveSites.Num() != 2) return;

	// �Ϸ�� ����Ʈ�� 2������ Ȯ��
	if (CompletedSites.Num() == 2)
	{
		// Ŭ���� Ȱ��ȭ ���·� ����
		GameState->SetCleanserActivated(true);

		// Phase2 �Ϸ�
		if (GameMode)
		{
			GameMode->ValidatePhaseCompletion();
		}
	}
}
