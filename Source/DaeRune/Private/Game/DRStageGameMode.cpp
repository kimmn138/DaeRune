// Copyright DaeRune


#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Actor/DRCleanserSite.h"
#include "Phase/DRPhaseBase.h"
#include "EngineUtils.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "Character/DRCharacter.h"

ADRStageGameMode::ADRStageGameMode()
{
	// �⺻ ����
	LobbyMapName = TEXT("LobbyMap");
	WipeoutDelayTime = 5.0f;
}

UClass* ADRStageGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (APlayerController* PC = Cast<APlayerController>(InController))
	{
		if (ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>())
		{
			EPlayerCharacterClass SelectedClass = PS->GetSelectedPlayerClass();

			if (PlayerCharacterClassInfo)
			{
				TSubclassOf<ADRCharacter>* BPClassPtr = PlayerCharacterClassInfo->CharacterBPClasses.Find(SelectedClass);
				if (BPClassPtr && *BPClassPtr)
				{
					return *BPClassPtr;
				}
			}
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
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

	// Multicast RPC로 모든 클라이언트에서 사운드 재생
	if (ADRStageGameState* StageGameState = GetGameState<ADRStageGameState>())
	{
		StageGameState->Multicast_PlayGameOverSound();
	}

	// 모든 플레이어에게 게임 오버 알림 (단일 순회)
	NotifyAllPlayersGameEnd(false);

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

	// Multicast RPC로 모든 클라이언트에서 사운드 재생
	if (ADRStageGameState* StageGameState = GetGameState<ADRStageGameState>())
	{
		StageGameState->Multicast_PlayGameClearSound();
	}

	// 모든 플레이어에게 게임 클리어 알림 (단일 순회)
	NotifyAllPlayersGameEnd(true);

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

	// 맵 전환 전 정리 작업 (부모 클래스의 공통 함수 사용)
	PrepareForTravel();

	UWorld* World = GetWorld();
	if (World)
	{
		bUseSeamlessTravel = true;
		World->ServerTravel(LobbyMapName + TEXT("?listen"));
	}

	// 플래그 리셋
	bIsWipeoutInProgress = false;
}

void ADRStageGameMode::NotifyAllPlayersGameEnd(bool bIsGameClear)
{
	if (!HasAuthority()) return;

	// 단일 순회로 UI 표시 + 오디오 정리 수행
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
		{
			// UI 표시
			if (bIsGameClear)
			{
				PC->Client_ShowGameClearUI();
			}
			else
			{
				PC->Client_ShowGameOverUI();
			}

			// 오디오 정리 (맵 전환 전 미리 수행)
			PC->ClientStopAllAudio();
		}
	}
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

	// 첫 번째 페이즈 시작 (클라이언트 초기화 대기를 위한 딜레이)
	if (PhaseInstances.Num() > 0)
	{
		// 클라이언트가 SeamlessTravel 후 오디오/UI 시스템을 초기화할 시간을 줌
		FTimerHandle PhaseStartTimer;
		GetWorldTimerManager().SetTimer(
			PhaseStartTimer,
			[this]()
			{
				StartPhase(0);
			},
			1.0f,  // 1초 딜레이
			false
		);
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

	// 페이즈 시작
	if (CurrentPhase)
	{
		CurrentPhase->OnPhaseStart();
	}

	// Phase 전환 완료 - Race Condition 방지 플래그 해제
	bIsTransitioningPhase = false;

	// 델리게이트 이벤트 호출
	// OnPhaseStarted();
}

void ADRStageGameMode::EndCurrentPhase()
{
	if (!HasAuthority() || !CachedGameState || !CurrentPhase) return;

	// Phase 전환 시작 - Race Condition 방지
	bIsTransitioningPhase = true;

	// 페이즈 완료 상태로 변경
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

	// Phase 전환 중이거나 게임 종료 처리 중이면 중복 호출 방지
	if (bIsTransitioningPhase || bIsWipeoutInProgress) return false;

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

