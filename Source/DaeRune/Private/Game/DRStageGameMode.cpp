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
#include "Game/DRGameInstance.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"

ADRStageGameMode::ADRStageGameMode()
{
	// 占썩본 占쏙옙占쏙옙
	LobbyMapName = TEXT("LobbyMap");
	WipeoutDelayTime = 5.0f;
}

UClass* ADRStageGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (APlayerController* PC = Cast<APlayerController>(InController))
	{
		// GameInstance?먯꽌 ??λ맂 ?좏깮 ?뺣낫 蹂듭썝 (留??꾪솚 諛⑹떇??愿怨꾩뾾???뺤떎??蹂댁〈??
		EPlayerCharacterClass SelectedClass = EPlayerCharacterClass::Gardener;

		if (ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>())
		{
			if (UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance()))
			{
				SelectedClass = GI->LoadPlayerClassSelection(PS->GetPlayerName());
// PlayerState???숆린??
				if (PS->GetSelectedPlayerClass() != SelectedClass)
				{
					PS->SetSelectedPlayerClass(SelectedClass);
				}
			}
		}

		if (UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(this))
		{
			TSubclassOf<ADRCharacter>* BPClassPtr = ClassInfo->CharacterBPClasses.Find(SelectedClass);
			if (BPClassPtr && *BPClassPtr)
			{
				return *BPClassPtr;
			}
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ADRStageGameMode::TriggerGameOver()
{
	if (!HasAuthority()) return;

	// ?대? 寃뚯엫 ?ㅻ쾭 泥섎━ 以묒씠硫?以묐났 ?몄텧 諛⑹?
	if (bIsWipeoutInProgress) return;

	bIsWipeoutInProgress = true;

	if (CurrentPhase && IsValid(CurrentPhase))
	{
		CurrentPhase->OnPhaseEnd();
	}

	// Multicast RPC濡?紐⑤뱺 ?대씪?댁뼵?몄뿉???ъ슫???ъ깮
	if (ADRStageGameState* StageGameState = GetGameState<ADRStageGameState>())
	{
		StageGameState->Multicast_PlayGameOverSound();
	}

	// 紐⑤뱺 ?뚮젅?댁뼱?먭쾶 寃뚯엫 ?ㅻ쾭 ?뚮┝ (?⑥씪 ?쒗쉶)
	NotifyAllPlayersGameEnd(false);

	// ?쎄컙???쒕젅????濡쒕퉬濡?蹂듦?
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

	// ?대? 寃뚯엫 ?ㅻ쾭 泥섎━ 以묒씠硫?以묐났 ?몄텧 諛⑹?
	if (bIsWipeoutInProgress) return;

	bIsWipeoutInProgress = true;

	if (CurrentPhase && IsValid(CurrentPhase))
	{
		CurrentPhase->OnPhaseEnd();
	}

	// Multicast RPC濡?紐⑤뱺 ?대씪?댁뼵?몄뿉???ъ슫???ъ깮
	if (ADRStageGameState* StageGameState = GetGameState<ADRStageGameState>())
	{
		StageGameState->Multicast_PlayGameClearSound();
	}

	// 紐⑤뱺 ?뚮젅?댁뼱?먭쾶 寃뚯엫 ?대━???뚮┝ (?⑥씪 ?쒗쉶)
	NotifyAllPlayersGameEnd(true);

	// ?쎄컙???쒕젅????濡쒕퉬濡?蹂듦?
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

	// ?꾩쨷 李멸? 李⑤떒
	BlockJoinInProgress();

	// GameState 캐占쏙옙
	CachedGameState = GetGameState<ADRStageGameState>();

	// 占쏙옙占쏙옙占쏙옙占쏙옙 클占쏙옙占쏙옙 占쏙옙占쏙옙트 占쌘듸옙 탐占쏙옙
	UWorld* World = GetWorld();
	if (World)
	{
		CleanserSites.Empty();

		// 占승그뤄옙 클占쏙옙占쏙옙 占쏙옙占쏙옙트 찾占쏙옙
		for (TActorIterator<ADRCleanserSite> It(World); It; ++It)
		{
			ADRCleanserSite* Site = *It;
			if (Site && Site->ActorHasTag(CleanserSiteTag))
			{
				CleanserSites.Add(Site);
			}
		}
	}

	// 占쏙옙占쏙옙占쏙옙 占시쏙옙占쏙옙 占십깍옙화
	InitializePhaseSystem();
}

void ADRStageGameMode::HandleWipeout()
{
	if (!HasAuthority()) return;

	// TODO: 占싻뱄옙 UI 표占쏙옙, 占싻뱄옙 占쏙옙占쏙옙 占쏙옙占?占쏙옙

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
		// ?ㅽ뀒?댁??먯꽌???꾩쨷 李멸? 李⑤떒!
		SessionsSubsystem->UpdateSessionJoinability(false);
	}
}

void ADRStageGameMode::ReturnToLobby()
{
	if (!HasAuthority()) return;

	// 留??꾪솚 ???뺣━ ?묒뾽 (遺紐??대옒?ㅼ쓽 怨듯넻 ?⑥닔 ?ъ슜)
	PrepareForTravel();

	UWorld* World = GetWorld();
	if (World)
	{
		bUseSeamlessTravel = true;
		World->ServerTravel(LobbyMapName + TEXT("?listen"));
	}

	// ?뚮옒洹?由ъ뀑
	bIsWipeoutInProgress = false;
}

void ADRStageGameMode::NotifyAllPlayersGameEnd(bool bIsGameClear)
{
	if (!HasAuthority()) return;

	// ?⑥씪 ?쒗쉶濡?UI ?쒖떆 + ?ㅻ뵒???뺣━ ?섑뻾
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
		{
			// UI ?쒖떆
			if (bIsGameClear)
			{
				PC->Client_ShowGameClearUI();
			}
			else
			{
				PC->Client_ShowGameOverUI();
			}

			// ?ㅻ뵒???뺣━ (留??꾪솚 ??誘몃━ ?섑뻾)
			PC->ClientStopAllAudio();
		}
	}
}

void ADRStageGameMode::InitializePhaseSystem()
{
	if (!HasAuthority()) return;

	// 페이즈 구조 개편: 사이트 맵 배치/활성화 모두 1개로 고정 (Phase1이 안전망으로 1개만 유지)
	if (CleanserSites.Num() < 1) return;

	// 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占싸쏙옙占싹쏙옙 占쏙옙占쏙옙
	PhaseInstances.Empty();

	// 占쏙옙占쏙옙占쏙옙 클占쏙옙占쏙옙占쏙옙觀占쏙옙占?占싸쏙옙占싹쏙옙 占쏙옙占쏙옙
	for (TSubclassOf<UDRPhaseBase> PhaseClass : PhaseClasses)
	{
		if (PhaseClass)
		{
			UDRPhaseBase* NewPhase = NewObject<UDRPhaseBase>(this, PhaseClass);

			// 占쏙옙占쏙옙占쏙옙 占십깍옙화 (GameMode, GameState 占쏙옙占쏙옙)
			NewPhase->Initialize(this, CachedGameState);

			// 클占쏙옙占쏙옙 占쏙옙占쏙옙트 占쏙옙占쏙옙 (占쏙옙占?占쏙옙占쏙옙占쏘가 占쏙옙占쏙옙)
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

	// 泥?踰덉㎏ ?섏씠利??쒖옉 (?대씪?댁뼵??珥덇린???湲곕? ?꾪븳 ?쒕젅??
	if (PhaseInstances.Num() > 0)
	{
		// ?대씪?댁뼵?멸? SeamlessTravel ???ㅻ뵒??UI ?쒖뒪?쒖쓣 珥덇린?뷀븷 ?쒓컙??以?
		FTimerHandle PhaseStartTimer;
		GetWorldTimerManager().SetTimer(
			PhaseStartTimer,
			[this]()
			{
				StartPhase(0);
			},
			1.0f,  // 1珥??쒕젅??
			false
		);
	}
}

void ADRStageGameMode::StartPhase(int32 PhaseIndex)
{
	if (!HasAuthority() || !CachedGameState) return;

	if (PhaseIndex < 0 || PhaseIndex >= PhaseInstances.Num()) return;

	// 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
	if (CurrentPhase)
	{
		// 占쏙옙占쏙옙 Phase占쏙옙 ActiveCleanserSites 占쏙옙占쏙옙
		TArray<TObjectPtr<ADRCleanserSite>> PreviousActiveSites = CurrentPhase->GetActiveCleanserSites();

		CurrentPhase->OnPhaseEnd();

		// 占쏙옙 Phase占쏙옙 占쏙옙占쏙옙
		if (PhaseIndex > 0 && PreviousActiveSites.Num() > 0)
		{
			UDRPhaseBase* NextPhase = PhaseInstances[PhaseIndex];
			if (NextPhase)
			{
				NextPhase->SetActiveCleanserSites(PreviousActiveSites);
			}
		}
	}

	// 占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
	CurrentPhase = PhaseInstances[PhaseIndex];

	// GameState 占쏙옙占쏙옙占쏙옙트
	CachedGameState->SetCurrentPhaseIndex(PhaseIndex);
	CachedGameState->SetCurrentPhaseState(EPhaseState::InProgress);

	// ?섏씠利??쒖옉
	if (CurrentPhase)
	{
		CurrentPhase->OnPhaseStart();
	}

	// Phase ?꾪솚 ?꾨즺 - Race Condition 諛⑹? ?뚮옒洹??댁젣
	bIsTransitioningPhase = false;

	// ?몃━寃뚯씠???대깽???몄텧
	// OnPhaseStarted();
}

void ADRStageGameMode::EndCurrentPhase()
{
	if (!HasAuthority() || !CachedGameState || !CurrentPhase) return;

	// Phase ?꾪솚 ?쒖옉 - Race Condition 諛⑹?
	bIsTransitioningPhase = true;

	// ?섏씠利??꾨즺 ?곹깭濡?蹂寃?
	CachedGameState->SetCurrentPhaseState(EPhaseState::Completed);

	// 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 처占쏙옙
	if (CurrentPhase)
	{
		CurrentPhase->OnPhaseEnd(); // PhaseBase占쏙옙占쏙옙 占쏙옙占쏙옙
	}

	// 占쏙옙占쏙옙占쏙옙占쏙옙트 占싹뤄옙 占싱븝옙트 호占쏙옙
	// OnPhaseCompleted();

	TransitionToNextPhase();
}

void ADRStageGameMode::TransitionToNextPhase()
{
	if (!HasAuthority() || !CachedGameState) return;

	int32 CurrentIndex = CachedGameState->GetCurrentPhaseIndex();
	int32 NextIndex = CurrentIndex + 1;

	// 占쏙옙占?占쏙옙占쏙옙占쏙옙 占싹뤄옙 체크
	if (NextIndex >= PhaseInstances.Num())
	{
		// 占쏙옙占쏙옙占쏙옙占쏙옙트 占쏙옙체 占싹뤄옙 占싱븝옙트 호占쏙옙
		TriggerGameClear();
		return;
	}

	// 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占?占쏙옙환
	StartPhase(NextIndex);
}

bool ADRStageGameMode::ValidatePhaseCompletion()
{
	if (!CurrentPhase || !CachedGameState) return false;

	// Phase ?꾪솚 以묒씠嫄곕굹 寃뚯엫 醫낅즺 泥섎━ 以묒씠硫?以묐났 ?몄텧 諛⑹?
	if (bIsTransitioningPhase || bIsWipeoutInProgress) return false;

	int32 CurrentPhaseIndex = CachedGameState->GetCurrentPhaseIndex();
	bool bIsCompleted = false;

	// 페이즈 구조 개편 (2개 페이즈):
	//   case 0 = New Phase1 (클렌저 확보 + 부품 회수 통합) → 부품 2개 설치 + 사이트 활성화
	//   case 1 = New Phase2 (= 기존 Phase3, 방어) → 모든 웨이브 클리어
	switch (CurrentPhaseIndex)
	{
	case 0: // New Phase1: 부품 2개 고정 설치 + 사이트 활성화
	{
		const int32 CollectedParts = CachedGameState->GetCollectedParts();
		const bool bActivated = CachedGameState->IsCleanserActivated();
		bIsCompleted = (CollectedParts >= 2) && bActivated;
	}
	break;

	case 1: // New Phase2 (= 기존 Phase3): 모든 웨이브 클리어
	{
		const int32 CurrentWaveNumber = CachedGameState->GetCurrentWaveNumber();
		const int32 TotalWaves = CachedGameState->GetTotalWaves();
		bIsCompleted = CurrentWaveNumber >= TotalWaves;
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


