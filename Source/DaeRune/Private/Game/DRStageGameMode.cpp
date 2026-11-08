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

	// 페이즈 시스템 초기화 (첫 페이즈는 시작 연출이 끝날 때 — FinishStageIntro)
	InitializePhaseSystem();

	// 시작 연출은 모든 플레이어가 로딩을 마친 뒤다 (각 클라가 ServerNotifyStageLoaded 로 알린다).
	// 끝까지 오지 않는 플레이어 때문에 모두가 로딩 화면에 갇히지 않게 상한을 둔다
	GetWorldTimerManager().SetTimer(StageIntroWaitTimeoutHandle, this, &ThisClass::HandleStageIntroWaitTimeout,
		StageIntroMaxWaitTime, false);
}

void ADRStageGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	// 로딩 중 · 연출 중 나간 플레이어를 더 기다리지 않는다 — 나가는 컨트롤러는 아직 목록에 있지만 파괴 중이라 건너뛴다
	APlayerController* ExitingPC = Cast<APlayerController>(Exiting);
	StageLoadedPlayers.Remove(ExitingPC);
	StageIntroPlayers.Remove(ExitingPC);
	TryStartStageIntro();
	TryFinishStageIntro();
}

void ADRStageGameMode::NotifyPlayerStageLoaded(APlayerController* PC)
{
	if (!HasAuthority() || !PC) return;

	StageLoadedPlayers.Add(PC);

	// 이미 시작했다 (기다림 상한을 넘겨 늦게 온 플레이어) — 그 클라는 복제된 진행 상태를 보고 혼자 연출을 튼다
	if (bStageIntroStarted)
	{
		UE_LOG(LogDR, Log, TEXT("[StageIntro] %s 가 늦게 로딩을 마쳤다 — 혼자 시작 연출"),
			PC->PlayerState ? *PC->PlayerState->GetPlayerName() : *PC->GetName());
		return;
	}

	TryStartStageIntro();
}

void ADRStageGameMode::TryStartStageIntro()
{
	if (!HasAuthority() || bStageIntroStarted || GetWorld()->bIsTearingDown) return;

	// 심리스 트래블로 넘어온 컨트롤러는 그 클라가 아직 로딩 중이어도 이미 목록에 있다 — 모두 알려 와야 시작한다
	int32 NumLoadedPlayers = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!IsValid(PC) || PC->IsActorBeingDestroyed() || !PC->PlayerState || PC->PlayerState->IsOnlyASpectator()) continue;

		if (!StageLoadedPlayers.Contains(PC)) return;
		++NumLoadedPlayers;
	}

	if (NumLoadedPlayers > 0)
	{
		UE_LOG(LogDR, Log, TEXT("[StageIntro] 플레이어 %d명 모두 로딩 완료 (%.1f초) — 시작 연출"), NumLoadedPlayers, GetWorld()->GetTimeSeconds());
		StartStageIntro();
	}
}

void ADRStageGameMode::StartStageIntro()
{
	bStageIntroStarted = true;
	GetWorldTimerManager().ClearTimer(StageIntroWaitTimeoutHandle);

	// 지금 로딩을 마친 플레이어들이 함께 연출을 본다 — 이들이 모두 끝내면 진행한다 (늦게 온 플레이어는 기다리지 않는다)
	StageIntroPlayers = StageLoadedPlayers;

	// 각 클라가 이 값을 보고 로딩 화면을 걷고 시작 연출을 튼다 (멀리서 내 캐릭터로 다가가 1인칭으로)
	if (CachedGameState)
	{
		CachedGameState->SetStageStartState(EDRStageStartState::Intro);
	}

	// 끝났다는 알림이 끝내 안 오는 클라가 있어도 진행하게 상한을 둔다 — 머무는 시간 + 다가오는 시간 + 여유
	// (여유 = 로딩 화면이 최소 표시 시간 때문에 늦게 걷히는 경우 · 네트워크 지연)
	constexpr float FinishGraceTime = 5.f;
	float IntroLength = 0.f;
	ForEachDRPlayerController([&IntroLength](ADRPlayerController* PC)
	{
		IntroLength = FMath::Max(IntroLength, PC->GetStageIntroHoldTime() + PC->GetStageIntroDuration());
	});
	GetWorldTimerManager().SetTimer(StageIntroFinishHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UE_LOG(LogDR, Warning, TEXT("[StageIntro] 시작 연출이 끝났다는 알림이 다 오지 않아 진행한다"));
		FinishStageIntro();
	}), IntroLength + FinishGraceTime, false);

	// 연출을 함께 볼 플레이어가 없으면 바로 진행
	TryFinishStageIntro();
}

void ADRStageGameMode::NotifyPlayerStageIntroFinished(APlayerController* PC)
{
	if (!HasAuthority() || !PC) return;

	StageIntroFinishedPlayers.Add(PC);
	TryFinishStageIntro();
}

void ADRStageGameMode::TryFinishStageIntro()
{
	if (!HasAuthority() || !bStageIntroStarted || bStageIntroFinished || GetWorld()->bIsTearingDown) return;

	// 함께 시작한 플레이어가 모두 연출을 끝냈을 때 — 로딩 화면이 걷히는 시점이 클라마다 달라 서버 타이머로는 어긋났다
	// (2026-11-08 PIE: 최소 표시 시간 때문에 늦게 출발한 연출이 끝나기 0.75초 전에 BGM · 페이즈가 시작됐다)
	for (const TWeakObjectPtr<APlayerController>& WeakPC : StageIntroPlayers)
	{
		const APlayerController* PC = WeakPC.Get();
		if (IsValid(PC) && !PC->IsActorBeingDestroyed() && !StageIntroFinishedPlayers.Contains(WeakPC)) return;
	}

	FinishStageIntro();
}

void ADRStageGameMode::FinishStageIntro()
{
	if (bStageIntroFinished) return;
	bStageIntroFinished = true;
	GetWorldTimerManager().ClearTimer(StageIntroFinishHandle);

	// 결과 처리 중이면 시작하지 않는다
	if (bIsWipeoutInProgress) return;

	// 스테이지 BGM 이 이걸 보고 나온다 (ADRBGMActor)
	if (CachedGameState)
	{
		CachedGameState->SetStageStartState(EDRStageStartState::Started);
	}

	// 첫 페이즈 — 모두가 조작을 시작하는 순간이다 (목표 · 시작 알림음 · 적 스폰).
	// 모두 도착한 뒤라 Phase1 의 시작 인원(InitialPlayerCount)도 전원으로 잡힌다. 치트로 먼저 넘겼으면 그대로 둔다
	if (!CurrentPhase && PhaseInstances.Num() > 0)
	{
		StartPhase(0);
	}
}

void ADRStageGameMode::HandleStageIntroWaitTimeout()
{
	if (bStageIntroStarted) return;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (IsValid(PC) && !StageLoadedPlayers.Contains(PC))
		{
			UE_LOG(LogDR, Warning, TEXT("[StageIntro] %s 가 %.0f초 안에 로딩을 마치지 못해 먼저 시작한다 (로딩을 마치면 혼자 연출을 본다)"),
				PC->PlayerState ? *PC->PlayerState->GetPlayerName() : *PC->GetName(), StageIntroMaxWaitTime);
		}
	}

	StartStageIntro();
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

	// 첫 번째 페이즈는 여기서 시작하지 않는다 — 모든 플레이어가 로딩을 마치고 시작 연출이 끝나면 FinishStageIntro 가 시작한다.
	// (예전에는 여기서 바로 시작해 원격 클라가 아직 로딩 중이었다 — Phase1 의 시작 인원이 호스트 1명으로 잡혔다)
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
