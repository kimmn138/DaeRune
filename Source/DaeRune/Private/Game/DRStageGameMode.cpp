// Copyright DaeRune


#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Kismet/GameplayStatics.h"
//#include "PlayLoop/DRPhaseBase.h"

ADRStageGameMode::ADRStageGameMode()
{
	// 기본 설정
	LobbyMapName = TEXT("StartupMap");
	WipeoutDelayTime = 3.0f;
}

void ADRStageGameMode::BeginPlay()
{
	Super::BeginPlay();

	// GameState 캐싱
	CachedGameState = GetGameState<ADRStageGameState>();

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

	//// 기존 페이즈 인스턴스 정리
	//PhaseInstances.Empty();

	//// 페이즈 클래스들로부터 인스턴스 생성
	//for (TSubclassOf<UDRPhaseBase> PhaseClass : PhaseClasses)
	//{
	//	if (PhaseClass)
	//	{
	//		UDRPhaseBase* NewPhase = NewObject<UDRPhaseBase>(this, PhaseClass);
	//		PhaseInstances.Add(NewPhase);
	//	}
	//}

	//// 첫 번째 페이즈로 시작
	//if (PhaseInstances.Num() > 0)
	//{
	//	StartPhase(0);
	//}
}

void ADRStageGameMode::StartPhase(int32 PhaseIndex)
{
	if (!HasAuthority() || !CachedGameState) return;

	//if (PhaseIndex < 0 || PhaseIndex >= PhaseInstances.Num()) return;

	//// 이전 페이즈 정리
	//if (CurrentPhase)
	//{
	//	// CurrentPhase->OnPhaseEnd(); // PhaseBase에서 구현
	//}

	//// 새 페이즈 설정
	//CurrentPhase = PhaseInstances[PhaseIndex];

	//// GameState 업데이트
	//CachedGameState->SetCurrentPhaseIndex(PhaseIndex);
	//CachedGameState->SetCurrentPhaseState(EPhaseState::InProgress);

	//// 페이즈 시작
	//if (CurrentPhase)
	//{
	//	// CurrentPhase->OnPhaseStart(); // PhaseBase에서 구현
	//}

	//// 이벤트 호출
	//OnPhaseStarted();
}

void ADRStageGameMode::EndCurrentPhase()
{
	//if (!HasAuthority() || !CachedGameState || !CurrentPhase) return;

	//// 페이즈 완료 상태로 변경
	//CachedGameState->SetCurrentPhaseState(EPhaseState::Completed);

	//// 페이즈 종료 처리
	//if (CurrentPhase)
	//{
	//	// CurrentPhase->OnPhaseEnd(); // PhaseBase에서 구현
	//}

	//// 완료 이벤트 호출
	//OnPhaseCompleted();
}

void ADRStageGameMode::TransitionToNextPhase()
{
	if (!HasAuthority() || !CachedGameState) return;

	//int32 CurrentIndex = CachedGameState->GetCurrentPhaseIndex();
	//int32 NextIndex = CurrentIndex + 1;

	//// 모든 페이즈 완료 체크
	//if (NextIndex >= PhaseInstances.Num())
	//{
	//	OnAllPhasesCompleted();
	//	return;
	//}

	//// 다음 페이즈로 전환
	//StartPhase(NextIndex);
}

//bool ADRStageGameMode::ValidatePhaseCompletion()
//{
//	if (!CurrentPhase || !CachedGameState) return false;
//
//	int32 CurrentPhaseIndex = CachedGameState->GetCurrentPhaseIndex();
//	bool bIsCompleted = false;
//
//	// 페이즈별 완료 조건 검증
//	switch (CurrentPhaseIndex)
//	{
//	case 0: // Phase 1: 클렌저 확보
//		bIsCompleted = CachedGameState->IsCleanserAreaSecured() &&
//			CachedGameState->GetRemainingEnemiesInArea() == 0;
//		break;
//
//	case 1: // Phase 2: 부품 회수
//		bIsCompleted = CachedGameState->GetCollectedParts() >= 4 &&
//			CachedGameState->IsCleanserActivated();
//		break;
//
//	case 2: // Phase 3: 방어
//		bIsCompleted = CachedGameState->GetCurrentWave() >= CachedGameState->GetTotalWaves();
//		break;
//
//	case 3: // Phase 4: 보스
//		bIsCompleted = CachedGameState->GetBossHealth() <= 0.0f;
//		break;
//
//	default:
//		break;
//	}
//
//	return bIsCompleted;
//}

