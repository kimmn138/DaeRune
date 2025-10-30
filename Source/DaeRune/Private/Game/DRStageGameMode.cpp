// Copyright DaeRune


#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Actor/DRCleanserSite.h"
#include "Phase/DRPhaseBase.h"
#include "EngineUtils.h"

ADRStageGameMode::ADRStageGameMode()
{
	// �⺻ ����
	LobbyMapName = TEXT("LobbyMap");
	WipeoutDelayTime = 3.0f;
}

void ADRStageGameMode::BeginPlay()
{
	Super::BeginPlay();

	// GameState ĳ��
	CachedGameState = GetGameState<ADRStageGameState>();

	// �������� Ŭ���� ����Ʈ �ڵ� Ž��
	UWorld* World = GetWorld();
	if (World)
	{
		CleanserSites.Empty();

		// �±׷� Ŭ���� ����Ʈ ã��
		for (TActorIterator<ADRCleanserSite> It(World); It; ++It)
		{
			ADRCleanserSite* Site = *It;
			if (Site && Site->ActorHasTag(CleanserSiteTag))
			{
				CleanserSites.Add(Site);
			}
		}
	}

	// ������ �ý��� �ʱ�ȭ
	InitializePhaseSystem();
}

void ADRStageGameMode::HandleWipeout()
{
	if (!HasAuthority()) return;

	// TODO: �й� UI ǥ��, �й� ���� ��� ��

	ReturnToLobby();
}

void ADRStageGameMode::ReturnToLobby()
{
	if (!HasAuthority()) return;

	UWorld* World = GetWorld();
	if (World)
	{
		bUseSeamlessTravel = true;
		World->ServerTravel(LobbyMapName + TEXT("?listen"));
	}

	// �÷��� ����
	bIsWipeoutInProgress = false;
}

void ADRStageGameMode::InitializePhaseSystem()
{
	if (!HasAuthority()) return;

	// Ŭ���� ����Ʈ ��ȿ�� ����
	if (CleanserSites.Num() < 3) return;

	// ���� ������ �ν��Ͻ� ����
	PhaseInstances.Empty();

	// ������ Ŭ������κ��� �ν��Ͻ� ����
	for (TSubclassOf<UDRPhaseBase> PhaseClass : PhaseClasses)
	{
		if (PhaseClass)
		{
			UDRPhaseBase* NewPhase = NewObject<UDRPhaseBase>(this, PhaseClass);

			// ������ �ʱ�ȭ (GameMode, GameState ����)
			NewPhase->Initialize(this, CachedGameState);

			// Ŭ���� ����Ʈ ���� (��� ����� ����)
			TArray<ADRCleanserSite*> SitesArray;
			for (const TObjectPtr<ADRCleanserSite>& Site : CleanserSites)
			{
				if (Site)
				{
					SitesArray.Add(Site.Get());
				}
			}
			NewPhase->SetCleanserSites(SitesArray);

			PhaseInstances.Add(NewPhase);
		}
	}

	// ù ��° ������� ����
	if (PhaseInstances.Num() > 0)
	{
		StartPhase(0);
	}
}

void ADRStageGameMode::StartPhase(int32 PhaseIndex)
{
	if (!HasAuthority() || !CachedGameState) return;

	if (PhaseIndex < 0 || PhaseIndex >= PhaseInstances.Num()) return;

	// ���� ������ ����
	if (CurrentPhase)
	{
		// ���� Phase�� ActiveCleanserSites ����
		TArray<TObjectPtr<ADRCleanserSite>> PreviousActiveSites = CurrentPhase->GetActiveCleanserSites();

		CurrentPhase->OnPhaseEnd();

		// �� Phase�� ����
		if (PhaseIndex > 0 && PreviousActiveSites.Num() > 0)
		{
			UDRPhaseBase* NextPhase = PhaseInstances[PhaseIndex];
			if (NextPhase)
			{
				NextPhase->SetActiveCleanserSites(PreviousActiveSites);
			}
		}
	}

	// �� ������ ����
	CurrentPhase = PhaseInstances[PhaseIndex];

	// GameState ������Ʈ
	CachedGameState->SetCurrentPhaseIndex(PhaseIndex);
	CachedGameState->SetCurrentPhaseState(EPhaseState::InProgress);

	// ������ ����
	if (CurrentPhase)
	{
		CurrentPhase->OnPhaseStart();
	}

	// ��������Ʈ �̺�Ʈ ȣ��
	// OnPhaseStarted();
}

void ADRStageGameMode::EndCurrentPhase()
{
	if (!HasAuthority() || !CachedGameState || !CurrentPhase) return;

	// ������ �Ϸ� ���·� ����
	CachedGameState->SetCurrentPhaseState(EPhaseState::Completed);

	// ������ ���� ó��
	if (CurrentPhase)
	{
		CurrentPhase->OnPhaseEnd(); // PhaseBase���� ����
	}

	// ��������Ʈ �Ϸ� �̺�Ʈ ȣ��
	// OnPhaseCompleted();

	UE_LOG(LogTemp, Warning, TEXT("Transitioning to next phase..."));  // �� �߰�
	TransitionToNextPhase();
}

void ADRStageGameMode::TransitionToNextPhase()
{
	if (!HasAuthority() || !CachedGameState) return;

	int32 CurrentIndex = CachedGameState->GetCurrentPhaseIndex();
	int32 NextIndex = CurrentIndex + 1;

	// ��� ������ �Ϸ� üũ
	if (NextIndex >= PhaseInstances.Num())
	{
		// ��������Ʈ ��ü �Ϸ� �̺�Ʈ ȣ��
		// OnAllPhasesCompleted();
		return;
	}

	// ���� ������� ��ȯ
	StartPhase(NextIndex);
}

bool ADRStageGameMode::ValidatePhaseCompletion()
{
	if (!CurrentPhase || !CachedGameState) return false;

	int32 CurrentPhaseIndex = CachedGameState->GetCurrentPhaseIndex();
	bool bIsCompleted = false;

	UE_LOG(LogTemp, Warning, TEXT("========== ValidatePhaseCompletion: Phase %d =========="), CurrentPhaseIndex);

	// ����� �Ϸ� ���� ����
	switch (CurrentPhaseIndex)
	{
	case 0: // Phase 1: Ŭ���� Ȯ��
	{
		bool bAreaSecured = CachedGameState->IsCleanserAreaSecured();
		int32 RemainingEnemies = CachedGameState->GetRemainingEnemiesInArea();

		UE_LOG(LogTemp, Warning, TEXT("Phase1 Check - AreaSecured: %s, RemainingEnemies: %d"),
			bAreaSecured ? TEXT("TRUE") : TEXT("FALSE"), RemainingEnemies);  // �� �߰�

		bIsCompleted = bAreaSecured && (RemainingEnemies == 0);
	}
	break;

	case 1: // Phase 2: ��ǰ ȸ��
	{
		int32 CollectedParts = CachedGameState->GetCollectedParts();
		bool bActivated = CachedGameState->IsCleanserActivated();

		UE_LOG(LogTemp, Warning, TEXT("Phase2 Check - CollectedParts: %d, Activated: %s"),
			CollectedParts, bActivated ? TEXT("TRUE") : TEXT("FALSE"));  // �� �߰�

		bIsCompleted = (CollectedParts >= 4) && bActivated;
	}
	break;

	case 2: // Phase 3: ���
	{
		int32 CurrentWave = CachedGameState->GetCurrentWave();
		int32 TotalWaves = CachedGameState->GetTotalWaves();

		UE_LOG(LogTemp, Warning, TEXT("Phase3 Check - CurrentWave: %d, TotalWaves: %d"),
			CurrentWave, TotalWaves);  // �� �߰�

		bIsCompleted = CurrentWave >= TotalWaves;
	}
	break;

	case 3: // Phase 4: ����
	{
		float BossHealth = CachedGameState->GetBossHealth();

		UE_LOG(LogTemp, Warning, TEXT("Phase4 Check - BossHealth: %f"), BossHealth);  // �� �߰�

		bIsCompleted = BossHealth <= 0.0f;
	}
	break;

	default:
		UE_LOG(LogTemp, Error, TEXT("ValidatePhaseCompletion: Invalid phase index %d"), CurrentPhaseIndex);  // �� �߰�
		break;
	}

	if (bIsCompleted)
	{
		UE_LOG(LogTemp, Warning, TEXT("========== Phase %d COMPLETED! =========="), CurrentPhaseIndex);  // �� �߰�
		EndCurrentPhase();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Phase %d not completed yet"), CurrentPhaseIndex);  // �� �߰�
	}

	return bIsCompleted;
}

