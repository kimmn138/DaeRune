// Copyright DaeRune


#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Actor/DRCleanserSite.h"
#include "Kismet/GameplayStatics.h"
#include "Phase/DRPhaseBase.h"
#include "EngineUtils.h"

ADRStageGameMode::ADRStageGameMode()
{
	// 기본 설정
	LobbyMapName = TEXT("LobbyMap");
	WipeoutDelayTime = 3.0f;
}

void ADRStageGameMode::BeginPlay()
{
	Super::BeginPlay();

	// GameState 캐싱
	CachedGameState = GetGameState<ADRStageGameState>();

	// 레벨에서 클렌저 사이트 자동 탐색
	UWorld* World = GetWorld();
	if (World)
	{
		CleanserSites.Empty();

		// 태그로 클렌저 사이트 찾기
		for (TActorIterator<ADRCleanserSite> It(World); It; ++It)
		{
			ADRCleanserSite* Site = *It;
			if (Site && Site->ActorHasTag(CleanserSiteTag))
			{
				CleanserSites.Add(Site);
			}
		}
	}

	// 페이즈 시스템 초기화
	InitializePhaseSystem();
}

void ADRStageGameMode::HandleWipeout()
{
	if (!HasAuthority()) return;

	// TODO: 패배 UI 표시, 패배 사운드 재생 등

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

	// 플래그 리셋
	bIsWipeoutInProgress = false;
}

void ADRStageGameMode::InitializePhaseSystem()
{
	if (!HasAuthority()) return;

	// 클렌저 사이트 유효성 검증
	if (CleanserSites.Num() < 3) return;

	// 기존 페이즈 인스턴스 정리
	PhaseInstances.Empty();

	// 페이즈 클래스들로부터 인스턴스 생성
	for (TSubclassOf<UDRPhaseBase> PhaseClass : PhaseClasses)
	{
		if (PhaseClass)
		{
			UDRPhaseBase* NewPhase = NewObject<UDRPhaseBase>(this, PhaseClass);

			// 페이즈 초기화 (GameMode, GameState 전달)
			NewPhase->Initialize(this, CachedGameState);

			// 클렌저 사이트 설정 (모든 페이즈가 공유)
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

	// 첫 번째 페이즈로 시작
	if (PhaseInstances.Num() > 0)
	{
		StartPhase(0);
	}
}

void ADRStageGameMode::StartPhase(int32 PhaseIndex)
{
	if (!HasAuthority() || !CachedGameState) return;

	if (PhaseIndex < 0 || PhaseIndex >= PhaseInstances.Num()) return;

	// 이전 페이즈 정리
	if (CurrentPhase)
	{
		// 이전 Phase의 ActiveCleanserSites 저장
		TArray<TObjectPtr<ADRCleanserSite>> PreviousActiveSites = CurrentPhase->GetActiveCleanserSites();

		CurrentPhase->OnPhaseEnd();

		// 새 Phase로 전달
		if (PhaseIndex > 0 && PreviousActiveSites.Num() > 0)
		{
			UDRPhaseBase* NextPhase = PhaseInstances[PhaseIndex];
			if (NextPhase)
			{
				NextPhase->SetActiveCleanserSites(PreviousActiveSites);
			}
		}
	}

	// 새 페이즈 설정
	CurrentPhase = PhaseInstances[PhaseIndex];

	// GameState 업데이트
	CachedGameState->SetCurrentPhaseIndex(PhaseIndex);
	CachedGameState->SetCurrentPhaseState(EPhaseState::InProgress);

	// 페이즈 시작
	if (CurrentPhase)
	{
		CurrentPhase->OnPhaseStart();
	}

	// 블루프린트 이벤트 호출
	// OnPhaseStarted();
}

void ADRStageGameMode::EndCurrentPhase()
{
	if (!HasAuthority() || !CachedGameState || !CurrentPhase) return;

	// 페이즈 완료 상태로 변경
	CachedGameState->SetCurrentPhaseState(EPhaseState::Completed);

	// 페이즈 종료 처리
	if (CurrentPhase)
	{
		CurrentPhase->OnPhaseEnd(); // PhaseBase에서 구현
	}

	// 블루프린트 완료 이벤트 호출
	// OnPhaseCompleted();

	UE_LOG(LogTemp, Warning, TEXT("Transitioning to next phase..."));  // ← 추가
	TransitionToNextPhase();
}

void ADRStageGameMode::TransitionToNextPhase()
{
	if (!HasAuthority() || !CachedGameState) return;

	int32 CurrentIndex = CachedGameState->GetCurrentPhaseIndex();
	int32 NextIndex = CurrentIndex + 1;

	// 모든 페이즈 완료 체크
	if (NextIndex >= PhaseInstances.Num())
	{
		// 블루프린트 전체 완료 이벤트 호출
		// OnAllPhasesCompleted();
		return;
	}

	// 다음 페이즈로 전환
	StartPhase(NextIndex);
}

bool ADRStageGameMode::ValidatePhaseCompletion()
{
	if (!CurrentPhase || !CachedGameState) return false;

	int32 CurrentPhaseIndex = CachedGameState->GetCurrentPhaseIndex();
	bool bIsCompleted = false;

	UE_LOG(LogTemp, Warning, TEXT("========== ValidatePhaseCompletion: Phase %d =========="), CurrentPhaseIndex);

	// 페이즈별 완료 조건 검증
	switch (CurrentPhaseIndex)
	{
	case 0: // Phase 1: 클렌저 확보
	{
		bool bAreaSecured = CachedGameState->IsCleanserAreaSecured();
		int32 RemainingEnemies = CachedGameState->GetRemainingEnemiesInArea();

		UE_LOG(LogTemp, Warning, TEXT("Phase1 Check - AreaSecured: %s, RemainingEnemies: %d"),
			bAreaSecured ? TEXT("TRUE") : TEXT("FALSE"), RemainingEnemies);  // ← 추가

		bIsCompleted = bAreaSecured && (RemainingEnemies == 0);
	}
	break;

	case 1: // Phase 2: 부품 회수
	{
		int32 CollectedParts = CachedGameState->GetCollectedParts();
		bool bActivated = CachedGameState->IsCleanserActivated();

		UE_LOG(LogTemp, Warning, TEXT("Phase2 Check - CollectedParts: %d, Activated: %s"),
			CollectedParts, bActivated ? TEXT("TRUE") : TEXT("FALSE"));  // ← 추가

		bIsCompleted = (CollectedParts >= 4) && bActivated;
	}
	break;

	case 2: // Phase 3: 방어
	{
		int32 CurrentWave = CachedGameState->GetCurrentWave();
		int32 TotalWaves = CachedGameState->GetTotalWaves();

		UE_LOG(LogTemp, Warning, TEXT("Phase3 Check - CurrentWave: %d, TotalWaves: %d"),
			CurrentWave, TotalWaves);  // ← 추가

		bIsCompleted = CurrentWave >= TotalWaves;
	}
	break;

	case 3: // Phase 4: 보스
	{
		float BossHealth = CachedGameState->GetBossHealth();

		UE_LOG(LogTemp, Warning, TEXT("Phase4 Check - BossHealth: %f"), BossHealth);  // ← 추가

		bIsCompleted = BossHealth <= 0.0f;
	}
	break;

	default:
		UE_LOG(LogTemp, Error, TEXT("ValidatePhaseCompletion: Invalid phase index %d"), CurrentPhaseIndex);  // ← 추가
		break;
	}

	if (bIsCompleted)
	{
		UE_LOG(LogTemp, Warning, TEXT("========== Phase %d COMPLETED! =========="), CurrentPhaseIndex);  // ← 추가
		EndCurrentPhase();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Phase %d not completed yet"), CurrentPhaseIndex);  // ← 추가
	}

	return bIsCompleted;
}

