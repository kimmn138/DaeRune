// Copyright DaeRune


#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Actor/DRCleanserSite.h"
#include "Phase/DRPhaseBase.h"
#include "EngineUtils.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "Character/DRCharacter.h"
#include "Game/DRGameInstance.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DaeRune/DRLogChannels.h"

ADRStageGameMode::ADRStageGameMode()
{
	// 기본 설정
	LobbyMapName = TEXT("LobbyMap");
	WipeoutDelayTime = 5.0f;
}

UClass* ADRStageGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (APlayerController* PC = Cast<APlayerController>(InController))
	{
		// GameInstance 에서 저장된 선택 정보 복원 (맵 전환 방식과 관계없이 확실히 보존됨)
		EPlayerCharacterClass SelectedClass = EPlayerCharacterClass::Gardener;

		if (ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>())
		{
			if (UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance()))
			{
				SelectedClass = GI->LoadPlayerClassSelection(PS);
				// PlayerState 에도 동기화
				if (PS->GetSelectedPlayerClass() != SelectedClass)
				{
					PS->SetSelectedPlayerClass(SelectedClass);
				}
			}
		}

		if (UClass* PawnClass = FindPlayerCharacterClass(SelectedClass))
		{
			return PawnClass;
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ADRStageGameMode::TriggerGameOver()
{
	EndStage(false);
}

void ADRStageGameMode::TriggerGameClear()
{
	EndStage(true);
}

void ADRStageGameMode::EndStage(bool bCleared)
{
	if (!HasAuthority()) return;

	// 이미 종료 처리 중이면 중복 호출 방지
	if (bIsWipeoutInProgress) return;
	bIsWipeoutInProgress = true;

	EndCurrentPhaseIfActive();

	// Multicast RPC로 모든 클라이언트에서 사운드 재생
	if (ADRStageGameState* StageGameState = GetGameState<ADRStageGameState>())
	{
		if (bCleared)
		{
			StageGameState->Multicast_PlayGameClearSound();
		}
		else
		{
			StageGameState->Multicast_PlayGameOverSound();
		}
	}

	// 모든 플레이어에게 결과 알림 (단일 순회)
	NotifyAllPlayersGameEnd(bCleared);

	// 약간의 딜레이 후 로비로 복귀
	GetWorldTimerManager().SetTimer(WipeoutTimerHandle, this, &ADRStageGameMode::ReturnToLobby, WipeoutDelayTime, false);
}

void ADRStageGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 도중 참가 차단
	SetJoinInProgress(false);   // 스테이지에서는 도중 참가 차단

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
	// 완료한 페이즈 수: 클리어면 전체, 아니면 진행 중이던 페이즈 이전까지 (표시용 — 재화로 환산하지 않는다)
	const int32 TotalPhases = PhaseClasses.Num();
	const int32 ClearedPhaseCount = bIsGameClear
		? TotalPhases
		: (CachedGameState ? FMath::Max(0, CachedGameState->GetCurrentPhaseIndex()) : 0);

	ForEachDRPlayerController([&](ADRPlayerController* PC)
	{
		// 재화 보상 통지를 결과 UI 표시보다 먼저 보낸다 (결과창이 반영된 지갑을 읽도록)
		FDRStageRewardReport Report;
		Report.StageId = StageId;
		Report.bGameClear = bIsGameClear;
		Report.ClearedPhaseCount = ClearedPhaseCount;

		if (const ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>())
		{
			Report.PlayedClass = PS->GetSelectedPlayerClass();
			Report.KillCount = PS->GetStageKillCount();
		}

		// 업적/클리어 조건은 클리어를 전제로 한다 (Plan2.md 12-2)
		if (bIsGameClear)
		{
			Report.AchievementIds = GlobalPendingAchievements;
			if (const TArray<FName>* PlayerAchievements = PendingAchievements.Find(PC))
			{
				for (const FName& Id : *PlayerAchievements)
				{
					Report.AchievementIds.AddUnique(Id);
				}
			}
		}

		PC->Client_GrantStageReward(Report);

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
	});
}

void ADRStageGameMode::NotifyPhasePlayerDied(APlayerState* DeadPlayerState)
{
	if (CurrentPhase)
	{
		CurrentPhase->NotifyPlayerDied(DeadPlayerState);
	}
}

void ADRStageGameMode::NotifyPhasePlayerLeft(APlayerState* LeftPlayerState)
{
	if (CurrentPhase)
	{
		CurrentPhase->NotifyPlayerLeft(LeftPlayerState);
	}
}

void ADRStageGameMode::InitializePhaseSystem()
{
	if (!HasAuthority()) return;

	// 페이즈 구조 개편: 사이트 맵 배치/활성화 모두 1개로 고정 (Phase1이 안전망으로 1개만 유지)
	// Plan6 §5.1: 스테이지2는 방4 설치대 1개만 사용하며 맵 구성에 따라 사이트가 없을 수도 있다.
	// 사이트 유무로 페이즈 시스템 자체를 막지 않고 경고만 남긴다 (사이트 의존 로직만 동작하지 않음).
	if (CleanserSites.Num() < 1)
	{
		UE_LOG(LogDR, Warning, TEXT("[Phase] CleanserSite 가 하나도 없습니다. 사이트 의존 로직은 동작하지 않습니다."));
	}

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

	// 첫 번째 페이즈 시작 (클라이언트 초기화 대기를 위한 딜레이)
	if (PhaseInstances.Num() > 0)
	{
		// 목표 데이터(FPhaseObjectiveData)는 복제 프로퍼티이므로 지연 없이 시작해도
		// 늦게 초기화된 클라이언트는 OnRep으로 따라잡는다
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

		EndCurrentPhaseIfActive();

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

	// 페이즈 시작
	if (CurrentPhase)
	{
		CurrentPhase->OnPhaseStart();
	}

	// Phase 전환 완료 - Race Condition 방지 플래그 해제
	bIsTransitioningPhase = false;
}

void ADRStageGameMode::EndCurrentPhase()
{
	if (!HasAuthority() || !CachedGameState || !CurrentPhase) return;

	// Phase 전환 시작 - Race Condition 방지
	bIsTransitioningPhase = true;

	// 목표 클리어 연출 알림 (모든 클라이언트 - 마지막 페이즈 클리어도 이 경로를 거침)
	CachedGameState->Multicast_ObjectiveCompleted();

	// 현재 페이즈 종료 처리
	EndCurrentPhaseIfActive();

	TransitionToNextPhase();
}

void ADRStageGameMode::EndCurrentPhaseIfActive()
{
	if (IsValid(CurrentPhase) && CurrentPhase->IsPhaseActive())
	{
		CurrentPhase->OnPhaseEnd();
	}
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
		TriggerGameClear();
		return;
	}

	// 다음 페이즈로 전환
	StartPhase(NextIndex);
}

bool ADRStageGameMode::ValidatePhaseCompletion()
{
	if (!CurrentPhase || !CachedGameState) return false;

	// Phase 전환 중이거나 게임 종료 처리 중이면 중복 호출 방지
	if (bIsTransitioningPhase || bIsWipeoutInProgress) return false;

	// 완료 판정은 각 페이즈가 스스로 수행 (인덱스 switch 하드코딩 제거)
	const bool bIsCompleted = CurrentPhase->IsCompleted();

	if (bIsCompleted)
	{
		EndCurrentPhase();
	}

	return bIsCompleted;
}



void ADRStageGameMode::GrantStageAchievement(FName AchievementId, ADRPlayerController* PC)
{
	if (!HasAuthority() || AchievementId.IsNone()) return;

	// 스테이지 종료 시 일괄 전송된다. 같은 Id 중복 보고는 여기서 걸러진다.
	if (PC)
	{
		PendingAchievements.FindOrAdd(PC).AddUnique(AchievementId);
	}
	else
	{
		GlobalPendingAchievements.AddUnique(AchievementId);
	}
}
