// Copyright DaeRune


#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Actor/DRCleanserSite.h"
#include "Phase/DRPhaseBase.h"
#include "EngineUtils.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Player/DRPlayerController.h"
#include "Sound/DRSoundManager.h"

ADRStageGameMode::ADRStageGameMode()
{
	// �⺻ ����
	LobbyMapName = TEXT("LobbyMap");
	WipeoutDelayTime = 5.0f;
}

void ADRStageGameMode::TriggerGameOver()
{
	if (!HasAuthority()) return;
    
	// 이미 게임 오버 처리 중이면 중복 호출 방지
	if (bIsWipeoutInProgress) return;
    
	bIsWipeoutInProgress = true;

	if (CurrentPhase && IsValid(CurrentPhase))
	{
		CurrentPhase->OnPhaseEnd();
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDRSoundManager* SM = GI->GetSubsystem<UDRSoundManager>())
		{
			SM->PlayGameOverSound();
		}
	}

	// 모든 플레이어에게 게임 오버 UI 표시
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
		{
			PC->Client_ShowGameOverUI();
		}
	}
    
	// 약간의 딜레이 후 로비로 복귀
	GetWorldTimerManager().SetTimer(
		WipeoutTimerHandle,
		this,
		&ADRStageGameMode::ReturnToLobby,
		WipeoutDelayTime,
		false
	);
}

void ADRStageGameMode::TriggerGameClear()
{
	if (!HasAuthority()) return;
    
	// 이미 게임 오버 처리 중이면 중복 호출 방지
	if (bIsWipeoutInProgress) return;
    
	bIsWipeoutInProgress = true;

	if (CurrentPhase && IsValid(CurrentPhase))
	{
		CurrentPhase->OnPhaseEnd();
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDRSoundManager* SM = GI->GetSubsystem<UDRSoundManager>())
		{
			SM->PlayGameClearSound();
		}
	}
    
	// 모든 플레이어에게 게임 클리어 UI 표시
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
		{
			PC->Client_ShowGameClearUI();
		}
	}
    
	// 약간의 딜레이 후 로비로 복귀
	GetWorldTimerManager().SetTimer(
		WipeoutTimerHandle,
		this,
		&ADRStageGameMode::ReturnToLobby,
		WipeoutDelayTime,
		false
	);
}

void ADRStageGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 도중 참가 차단
	BlockJoinInProgress();

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

void ADRStageGameMode::BlockJoinInProgress()
{
	if (!HasAuthority()) return;

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance) return;

	UMultiplayerSessionsSubsystem* SessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	if (SessionsSubsystem)
	{
		// 스테이지에서는 도중 참가 차단!
		SessionsSubsystem->UpdateSessionJoinability(false);
	}
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
		TriggerGameClear();
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

	// ����� �Ϸ� ���� ����
	switch (CurrentPhaseIndex)
	{
	case 0: // Phase 1: Ŭ���� Ȯ��
	{
		bool bAreaSecured = CachedGameState->IsCleanserAreaSecured();
		int32 RemainingEnemies = CachedGameState->GetRemainingEnemiesInArea();

		bIsCompleted = bAreaSecured && (RemainingEnemies == 0);
	}
	break;

	case 1: // Phase 2: ��ǰ ȸ��
	{
		int32 CollectedParts = CachedGameState->GetCollectedParts();
		bool bActivated = CachedGameState->IsCleanserActivated();

		bIsCompleted = (CollectedParts >= 4) && bActivated;
	}
	break;

	case 2: // Phase 3: ���
	{
		int32 CurrentWaveNumber = CachedGameState->GetCurrentWaveNumber();
		int32 TotalWaves = CachedGameState->GetTotalWaves();

		bIsCompleted = CurrentWaveNumber >= TotalWaves;
	}
	break;

	case 3: // Phase 4: ����
	{
		float BossHealth = CachedGameState->GetBossHealth();

		bIsCompleted = BossHealth <= 0.0f;
	}
	break;

	default:
		break;
	}

	if (bIsCompleted)
	{
		EndCurrentPhase();
	}

	return bIsCompleted;
}

