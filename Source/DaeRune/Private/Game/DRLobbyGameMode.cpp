// Copyright DaeRune


#include "Game/DRLobbyGameMode.h"
#include "Game/DRLobbyGameState.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "MultiplayerSessionsSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Actor/DRWaitingRoomCameraActor.h"
#include "Character/DRCharacter.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRGameInstance.h"

ADRLobbyGameMode::ADRLobbyGameMode()
{
	WipeoutDelayTime = 2.0f;
}

void ADRLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(NewPlayer))
	{
		ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();

		// ?湲곗떎 ?곹깭?????щ’ 諛곗튂 + ?붿뒪?뚮젅??罹먮┃???ㅽ룿
		if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
		{
			FTimerHandle SlotTimerHandle;
			GetWorldTimerManager().SetTimer(
				SlotTimerHandle,
				[this, DRPC]()
				{
					if (!IsValid(DRPC)) return;
					AssignPlayerToSlot(DRPC);

					// ?붿뒪?뚮젅??罹먮┃???ㅽ룿
					ADRPlayerState* PS = DRPC->GetPlayerState<ADRPlayerState>();
					int32* SlotIdx = PlayerSlotMap.Find(DRPC);
					if (PS && SlotIdx)
					{
						SpawnDisplayCharacter(DRPC, PS->GetSelectedPlayerClass(), *SlotIdx);
					}

					// 怨좎젙 移대찓?쇰줈 ViewTarget ?ㅼ젙
					if (WaitingRoomCamera)
					{
						DRPC->ClientSetWaitingRoomView(WaitingRoomCamera);
					}
				},
				0.5f, false
			);
		}
		else
		{
			// FreeRoam ?곹깭: 湲곗〈 蹂듭썝 濡쒖쭅
			FTimerHandle RestoreTimerHandle;
			GetWorldTimerManager().SetTimer(
				RestoreTimerHandle,
				[DRPC]()
				{
					if (!IsValid(DRPC)) return;

					DRPC->ClientStopSpectating();

					if (APawn* ControlledPawn = DRPC->GetPawn())
					{
						if (UCharacterMovementComponent* MovementComp = Cast<UCharacterMovementComponent>(ControlledPawn->GetMovementComponent()))
						{
							MovementComp->SetMovementMode(MOVE_Walking);
							MovementComp->SetComponentTickEnabled(true);
						}

						if (UCapsuleComponent* CapsuleComp = Cast<UCapsuleComponent>(ControlledPawn->GetRootComponent()))
						{
							CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
						}
					}
				},
				0.5f,
				false
			);
		}
	}

	if (GameState)
	{
		int32 NumberOfPlayers = GameState.Get()->PlayerArray.Num();

		if (GEngine)
		{
APlayerState* PlayerState = NewPlayer->GetPlayerState<APlayerState>();
			if (PlayerState)
			{
				FString PlayerName = PlayerState->GetPlayerName();
}
		}
	}
}

void ADRLobbyGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);
if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(C))
	{
		ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();

		FTimerHandle RestoreTimerHandle;
		GetWorldTimerManager().SetTimer(
			RestoreTimerHandle,
			[this, DRPC, LGS]()
			{
				if (!IsValid(DRPC)) return;

				// 愿??紐⑤뱶 媛뺤젣 醫낅즺
				DRPC->ClientStopSpectating();

				if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
				{
					// SeamlessTravel濡?媛?몄삩 Pawn ?뺣━ (?湲곗떎?먯꽌???붿뒪?뚮젅???ъ슜)
					if (APawn* TravelPawn = DRPC->GetPawn())
					{
						DRPC->UnPossess();
						TravelPawn->Destroy();
					}

					// ?湲곗떎 紐⑤뱶: ?щ’ 諛곗튂 + ?붿뒪?뚮젅??罹먮┃??+ 怨좎젙 移대찓??
					AssignPlayerToSlot(DRPC);

					ADRPlayerState* PS = DRPC->GetPlayerState<ADRPlayerState>();
					int32* SlotIdx = PlayerSlotMap.Find(DRPC);
					if (PS && SlotIdx)
					{
						// PlayerController 罹먯떆?먯꽌 ?좏깮 ?대옒??蹂듭썝 (Seamless Travel ??PlayerState ?좎떎 諛⑹?)
						EPlayerCharacterClass CachedClass = DRPC->GetCachedSelectedClass();
						if (PS->GetSelectedPlayerClass() != CachedClass)
						{
							PS->SetSelectedPlayerClass(CachedClass);
						}
						SpawnDisplayCharacter(DRPC, CachedClass, *SlotIdx);
					}

					if (WaitingRoomCamera)
					{
						DRPC->ClientSetWaitingRoomView(WaitingRoomCamera);
					}
				}
				else
				{
					// ?먯쑀 議곗옉 紐⑤뱶: 湲곗〈 蹂듭썝 濡쒖쭅
					if (APawn* ControlledPawn = DRPC->GetPawn())
					{
						if (UCharacterMovementComponent* MovementComp = Cast<UCharacterMovementComponent>(ControlledPawn->GetMovementComponent()))
						{
							MovementComp->SetMovementMode(MOVE_Walking);
							MovementComp->SetComponentTickEnabled(true);
						}

						if (UCapsuleComponent* CapsuleComp = Cast<UCapsuleComponent>(ControlledPawn->GetRootComponent()))
						{
							CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
						}
					}
				}
			},
			0.5f, false
		);
	}
}

void ADRLobbyGameMode::Logout(AController* Exiting)
{
	// ?붿뒪?뚮젅??罹먮┃???쒓굅
	DestroyDisplayCharacter(Exiting);

	// ?щ’ 留ㅽ븨?먯꽌 ?쒓굅
	if (PlayerSlotMap.Contains(Exiting))
	{
		PlayerSlotMap.Remove(Exiting);

		// ?湲곗떎 ?곹깭硫??⑥? ?뚮젅?댁뼱 ?щ같移?
		ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
		if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
		{
			RepositionAllPlayers();
		}
	}

	Super::Logout(Exiting);

	APlayerState* PlayerState = Exiting->GetPlayerState<APlayerState>();
	if (PlayerState)
	{
		int32 NumberOfPlayers = GameState.Get()->PlayerArray.Num();
		if (GEngine)
		{
FString PlayerName = PlayerState->GetPlayerName();
}
	}
}

void ADRLobbyGameMode::TravelToStage(const FString& StageMapName, ADRPlayerController* Requester)
{
	// 占쏙옙占쏙옙 체크
	if (!HasAuthority()) return;

	// 호占쏙옙트 占쏙옙占쏙옙 체크
	if (!Requester || !Requester->IsLocalController()) return;

	// 占쏙옙 占싱몌옙 占쏙옙효占쏙옙
	if (StageMapName.IsEmpty()) return;

	ExecuteTravel(StageMapName);
}

void ADRLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	FindWaitingRoomActors();
	AllowJoinInProgress();
}

void ADRLobbyGameMode::HandleWipeout()
{
	if (!HasAuthority()) return;

	// TODO: 占쏙옙占쏙옙 UI 표占쏙옙 (占싸븝옙占?占쏙옙占쏙옙占쏙옙 UI)

	RestartLobby();
}

void ADRLobbyGameMode::AllowJoinInProgress()
{
	if (!HasAuthority()) return;

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance) return;

	UMultiplayerSessionsSubsystem* SessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	if (SessionsSubsystem)
	{
		// 占싸비에쇽옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占?
		SessionsSubsystem->UpdateSessionJoinability(true);
	}
}

void ADRLobbyGameMode::RestartLobby()
{
	if (!HasAuthority()) return;

	UWorld* World = GetWorld();
	if (World)
	{
		FString CurrentMapName = World->GetMapName();

		// PIE(Play In Editor) 占쏙옙占쏙옙占싫쏙옙 占쏙옙占쏙옙
		// PIE占쏙옙占쏙옙占쏙옙 "UEDPIE_0_MapName" 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
		CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);

		bUseSeamlessTravel = true;

		// 占쏙옙占쏙옙 占쏙옙 占쏙옙占쏙옙占?
		World->ServerTravel(CurrentMapName + TEXT("?listen"));
	}

	// 占시뤄옙占쏙옙 占쏙옙占쏙옙
	bIsWipeoutInProgress = false;
}

void ADRLobbyGameMode::PowerOn(ADRPlayerController* Requester)
{
	if (!HasAuthority()) return;

	// ?몄뒪??沅뚰븳 泥댄겕
	if (!Requester || !Requester->IsLocalController()) return;

	ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
	if (!LGS) return;

	// ?湲곗떎 ?곹깭?먯꽌留?媛??
	if (LGS->GetLobbyState() != ELobbyState::WaitingRoom) return;

	// 모든 비호스트 플레이어가 준비 상태여야 PowerOn 가능
	if (!LGS->AreAllNonHostPlayersReady()) return;

	// 1. ?몄뀡 李멸? 李⑤떒
	BlockJoinInProgress();

	// 2. ?꾪솚 ?곹깭濡?蹂寃?
	LGS->SetLobbyState(ELobbyState::Transitioning);

	// 3. ?붿뒪?뚮젅??罹먮┃???쒓굅
	DestroyAllDisplayCharacters();

	// 4. 紐⑤뱺 ?뚮젅?댁뼱?먭쾶 吏꾩쭨 罹먮┃???ㅽ룿 + Possess
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get());
		if (!PC) continue;

		ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>();
		if (!PS) continue;

		// ?좏깮???대옒?ㅼ쓽 BP ?ㅽ룿
		UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(this);
		if (!ClassInfo) continue;
		TSubclassOf<ADRCharacter>* BPClassPtr = ClassInfo->CharacterBPClasses.Find(PS->GetSelectedPlayerClass());
		if (!BPClassPtr || !*BPClassPtr) continue;

		int32* SlotIdx = PlayerSlotMap.Find(PC);
		FVector SpawnLoc = (SlotIdx && WaitingRoomSlots.IsValidIndex(*SlotIdx))
			? WaitingRoomSlots[*SlotIdx]->GetActorLocation()
			: FVector::ZeroVector;
		FRotator SpawnRot = WaitingRoomCamera
			? WaitingRoomCamera->GetCharacterFacingRotation()
			: FRotator::ZeroRotator;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ADRCharacter* NewPawn = GetWorld()->SpawnActor<ADRCharacter>(*BPClassPtr, FTransform(SpawnRot, SpawnLoc), SpawnParams);
		if (!NewPawn) continue;

		// Possess (理쒖큹 1?? 源쒕묀???놁쓬)
		PC->Possess(NewPawn);

		// ?대룞 ?쒖꽦??
		NewPawn->SetReplicateMovement(true);
		if (UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(NewPawn->GetMovementComponent()))
		{
			CMC->SetMovementMode(MOVE_Walking);
		}

		// 移대찓???꾪솚 紐낅졊 (Client RPC)
		PC->ClientStartCameraTransitionToCharacter();
	}

	// 5. ?꾪솚 ?꾨즺 ??FreeRoam?쇰줈 ?곹깭 蹂寃?(移대찓???곗텧 ?쒓컙留뚰겮 ?쒕젅??
	FTimerHandle TransitionTimerHandle;
	GetWorldTimerManager().SetTimer(
		TransitionTimerHandle,
		[LGS]()
		{
			if (IsValid(LGS))
			{
				LGS->SetLobbyState(ELobbyState::FreeRoam);
			}
		},
		2.0f, // 移대찓???꾪솚 ?쒓컙
		false
	);
}

void ADRLobbyGameMode::SetPlayerReady(ADRPlayerController* Player, bool bReady)
{
	if (!HasAuthority() || !Player) return;

	ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
	if (!LGS || LGS->GetLobbyState() != ELobbyState::WaitingRoom) return;

	ADRPlayerState* PS = Player->GetPlayerState<ADRPlayerState>();
	if (!PS) return;

	// 호스트는 준비 토글 대상이 아님 (호스트 버튼 = PowerOn)
	if (LGS->IsPlayerHost(PS)) return;

	PS->SetReady(bReady);

	// 모든 클라이언트/호스트 대기실 UI 갱신
	BroadcastRefreshWaitingRoomUI();
}

void ADRLobbyGameMode::KickPlayer(ADRPlayerController* Requester, ADRPlayerController* TargetPlayer)
{
	if (!HasAuthority()) return;

	// ?몄뒪??沅뚰븳 泥댄겕
	if (!Requester || !Requester->IsLocalController()) return;

	// ?寃??좏슚??泥댄겕
	if (!TargetPlayer) return;

	// ?먭린 ?먯떊? ??遺덇?
	if (Requester == TargetPlayer) return;

	// ?湲곗떎 ?곹깭?먯꽌留?媛??
	ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
	if (!LGS || LGS->GetLobbyState() != ELobbyState::WaitingRoom) return;

	// ?붿뒪?뚮젅??罹먮┃??+ ?щ’?먯꽌 ?쒓굅
	DestroyDisplayCharacter(TargetPlayer);
	PlayerSlotMap.Remove(TargetPlayer);

	// ?대씪?댁뼵?몄뿉寃????뚮┝ ??硫붿씤 硫붾돱濡??대룞
	TargetPlayer->ClientKicked(TEXT("You have been kicked by the host."));

	// ?섎㉧吏 ?뚮젅?댁뼱 ?щ같移?(?대??먯꽌 BroadcastRefreshWaitingRoomUI ?몄텧)
	RepositionAllPlayers();
}

void ADRLobbyGameMode::BlockJoinInProgress()
{
	if (!HasAuthority()) return;

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance) return;

	UMultiplayerSessionsSubsystem* SessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	if (SessionsSubsystem)
	{
		SessionsSubsystem->UpdateSessionJoinability(false);
	}
}

void ADRLobbyGameMode::FindWaitingRoomActors()
{
	// 移대찓???먯깋
	for (TActorIterator<ADRWaitingRoomCameraActor> It(GetWorld()); It; ++It)
	{
		WaitingRoomCamera = *It;
		break; // ?섎굹留??꾩슂
	}

	// "WaitingRoomSlot" ?쒓렇媛 ?덈뒗 ATargetPoint ?먯깋
	WaitingRoomSlots.Empty();
	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		for (const FName& Tag : It->Tags)
		{
			if (Tag.ToString().StartsWith(TEXT("WaitingRoomSlot")))
			{
				WaitingRoomSlots.Add(*It);
				break;
			}
		}
	}

	// ?쒓렇 suffix ?レ옄 湲곗? ?뺣젹 (WaitingRoomSlot_0, WaitingRoomSlot_1, ...)
	WaitingRoomSlots.Sort([](const TObjectPtr<AActor>& A, const TObjectPtr<AActor>& B)
	{
		auto GetSlotIndex = [](const AActor* Actor) -> int32
		{
			for (const FName& Tag : Actor->Tags)
			{
				FString TagStr = Tag.ToString();
				if (TagStr.StartsWith(TEXT("WaitingRoomSlot_")))
				{
					return FCString::Atoi(*TagStr.RightChop(16));
				}
			}
			return MAX_int32;
		};
		return GetSlotIndex(A.Get()) < GetSlotIndex(B.Get());
	});
if (WaitingRoomSlots.Num() == 0)
	{
}
}

void ADRLobbyGameMode::AssignPlayerToSlot(AController* Player)
{
	if (WaitingRoomSlots.Num() == 0) return;

	int32 SlotIndex = NextAvailableSlot++;
	PlayerSlotMap.Add(Player, SlotIndex);

	// PlayerState??沅뚯쐞???щ’ ?몃뜳???ㅼ젙 (?대씪?댁뼵?몄뿉 蹂듭젣??
	if (APlayerController* PC = Cast<APlayerController>(Player))
	{
		if (ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>())
		{
			PS->SetWaitingRoomSlotIndex(SlotIndex);
		}
	}

	if (APawn* Pawn = Player->GetPawn())
	{
		PositionPawnAtSlot(Pawn, SlotIndex);
	}
}

void ADRLobbyGameMode::PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex)
{
	if (!Pawn || WaitingRoomSlots.Num() == 0) return;

	// ?щ’ ?몃뜳???대옩??(踰붿쐞 珥덇낵 ??留덉?留??щ’ ?ъ슜)
	int32 ClampedIndex = FMath::Clamp(SlotIndex, 0, WaitingRoomSlots.Num() - 1);
	AActor* SlotActor = WaitingRoomSlots[ClampedIndex];
	if (!SlotActor) return;

	// 移대찓?쇰? ?ν븯???뚯쟾 怨꾩궛
	FRotator FacingRotation = FRotator::ZeroRotator;
	if (WaitingRoomCamera)
	{
		FacingRotation = WaitingRoomCamera->GetCharacterFacingRotation();
	}

	FVector SlotLocation = SlotActor->GetActorLocation();

	// CMC ?꾩튂 由ы뵆由ъ??댁뀡 鍮꾪솢?깊솕 (unreliable ?꾩튂 蹂댁젙??Multicast 寃곌낵瑜???뼱?곕뒗 寃?諛⑹?)
	Pawn->SetReplicateMovement(false);

	// Multicast RPC濡?紐⑤뱺 ?붾뱶?ъ씤?몄뿉??吏곸젒 ?꾩튂 ?ㅼ젙 (CMC ?고쉶)
	if (ADRCharacter* DRChar = Cast<ADRCharacter>(Pawn))
	{
		DRChar->MulticastTeleportToSlot(SlotLocation, FacingRotation);
	}

	// CMC 鍮꾪솢?깊솕 (?쒕쾭?먯꽌留????대룞 ?낅젰 李⑤떒??
	if (UCharacterMovementComponent* MovementComp =
		Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()))
	{
		MovementComp->StopMovementImmediately();
		MovementComp->DisableMovement();
	}
}

void ADRLobbyGameMode::RepositionAllPlayers()
{
	if (WaitingRoomSlots.Num() == 0) return;

	// 湲곗〈 留ㅽ븨 ?섏쭛 (?몄뒪???곗꽑)
	TArray<AController*> OrderedPlayers;

	// ?몄뒪?몃? 癒쇱? 李얘린
	for (auto& Pair : PlayerSlotMap)
	{
		if (ADRPlayerController* PC = Cast<ADRPlayerController>(Pair.Key))
		{
			if (PC->IsLocalController() && HasAuthority())
			{
				OrderedPlayers.Insert(Pair.Key, 0); // ?몄뒪?몃? 留??욎뿉
				continue;
			}
		}
		OrderedPlayers.Add(Pair.Key);
	}

	// 留ㅽ븨 ?ш뎄??
	PlayerSlotMap.Empty();
	NextAvailableSlot = 0;

	for (AController* Player : OrderedPlayers)
	{
		AssignPlayerToSlot(Player);
	}

	// ?붿뒪?뚮젅??罹먮┃???꾩튂 媛깆떊 (NetMulticast RPC濡?紐⑤뱺 ?대씪?댁뼵?몄뿉 ?꾪뙆)
	for (auto& Pair : DisplayCharacterMap)
	{
		if (ADRCharacter* Display = Pair.Value.Get())
		{
			int32* SlotIdx = PlayerSlotMap.Find(Pair.Key);
			if (SlotIdx && WaitingRoomSlots.IsValidIndex(*SlotIdx))
			{
				FVector Loc = WaitingRoomSlots[*SlotIdx]->GetActorLocation();
				FRotator Rot = WaitingRoomCamera
					? WaitingRoomCamera->GetCharacterFacingRotation()
					: FRotator::ZeroRotator;

				// 罹≪뒓 諛섎넂?대쭔??Z 蹂댁젙 (TargetPoint媛 諛붾떏 ?덈꺼??寃쎌슦 諛붾떏 ?ル┝ 諛⑹?)
				if (UCapsuleComponent* Capsule = Display->GetCapsuleComponent())
				{
					Loc.Z += Capsule->GetScaledCapsuleHalfHeight();
				}

				// NetMulticast RPC濡?紐⑤뱺 ?붾뱶?ъ씤?몄뿉???꾩튂 媛깆떊
				Display->MulticastTeleportToSlot(Loc, Rot);
			}
		}
	}

	// 紐⑤뱺 ?대씪?댁뼵?몄쓽 ?湲곗떎 UI 媛깆떊 (Client RPC ?ъ슜)
	BroadcastRefreshWaitingRoomUI();
}

void ADRLobbyGameMode::BroadcastRefreshWaitingRoomUI()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
		{
			PC->ClientRefreshWaitingRoomUI();
		}
	}
}

void ADRLobbyGameMode::RespawnPlayerWithClass(ADRPlayerController* PC, EPlayerCharacterClass NewClass)
{
	if (!HasAuthority() || !PC) return;

	ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();

	// ?湲곗떎: ?붿뒪?뚮젅?대쭔 援먯껜 (Possess/UnPossess ?놁쓬 ??源쒕묀??洹쇰낯 ?쒓굅)
	if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
	{
		UpdateDisplayCharacter(PC, NewClass);
		BroadcastRefreshWaitingRoomUI();
		return;
	}

	// FreeRoam: 湲곗〈 ??援먯껜 濡쒖쭅 (吏꾩쭨 Possess ?꾩슂)
	UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(this);
	if (!ClassInfo) return;
	TSubclassOf<ADRCharacter>* BPClassPtr = ClassInfo->CharacterBPClasses.Find(NewClass);
	if (!BPClassPtr || !*BPClassPtr) return;

	APawn* OldPawn = PC->GetPawn();
	FTransform SpawnTransform = OldPawn ? OldPawn->GetActorTransform() : FTransform::Identity;

	if (OldPawn)
	{
		PC->UnPossess();
		OldPawn->Destroy();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ADRCharacter* NewPawn = GetWorld()->SpawnActor<ADRCharacter>(*BPClassPtr, SpawnTransform, SpawnParams);
	if (!NewPawn) return;

	// ?대퉴由ы떚 ?꾩쟾 ?대━??
	ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>();
	if (PS)
	{
		UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
		if (ASC)
		{
			ASC->CancelAllAbilities();
			ASC->ClearAllAbilities();

			if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(ASC))
			{
				DRASC->bStartupAbilitiesGiven = false;
				DRASC->ClearInputTagCache();
			}
		}
	}

	PC->Possess(NewPawn);
}

UClass* ADRLobbyGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();

	// ?湲곗떎/?꾪솚 以묒뿉??Pawn ?ㅽ룿 ?듭젣 (?붿뒪?뚮젅??罹먮┃???ъ슜)
	if (LGS && (LGS->GetLobbyState() == ELobbyState::WaitingRoom
			 || LGS->GetLobbyState() == ELobbyState::Transitioning))
	{
		return nullptr;
	}

	// FreeRoam: 湲곗〈 濡쒖쭅 (?좏깮???대옒?ㅼ쓽 BP 諛섑솚)
	if (APlayerController* PC = Cast<APlayerController>(InController))
	{
		if (ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>())
		{
			EPlayerCharacterClass SelectedClass = PS->GetSelectedPlayerClass();

			if (UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(this))
			{
				TSubclassOf<ADRCharacter>* BPClassPtr = ClassInfo->CharacterBPClasses.Find(SelectedClass);
				if (BPClassPtr && *BPClassPtr)
				{
					return *BPClassPtr;
				}
			}
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ADRLobbyGameMode::SpawnDisplayCharacter(AController* Player, EPlayerCharacterClass CharClass, int32 SlotIndex)
{
	if (!Player) return;
	UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(this);
	if (!ClassInfo) return;
	TSubclassOf<ADRCharacter>* BPClassPtr = ClassInfo->CharacterBPClasses.Find(CharClass);
	if (!BPClassPtr || !*BPClassPtr) return;

	if (WaitingRoomSlots.Num() == 0) return;

	// ?щ’ ?꾩튂/?뚯쟾 怨꾩궛
	int32 ClampedIndex = FMath::Clamp(SlotIndex, 0, WaitingRoomSlots.Num() - 1);
	FVector Location = WaitingRoomSlots[ClampedIndex]->GetActorLocation();
	FRotator Rotation = WaitingRoomCamera
		? WaitingRoomCamera->GetCharacterFacingRotation()
		: FRotator::ZeroRotator;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ADRCharacter* Display = GetWorld()->SpawnActor<ADRCharacter>(*BPClassPtr, FTransform(Rotation, Location), Params);
	if (!Display) return;

	// ?대룞 鍮꾪솢?깊솕 (?붿뒪?뚮젅?댁슜)
	if (UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(Display->GetMovementComponent()))
	{
		CMC->StopMovementImmediately();
		CMC->DisableMovement();
	}

	// 由ы뵆由ъ??댄듃 ?대룞 鍮꾪솢?깊솕
	Display->SetReplicateMovement(false);

	DisplayCharacterMap.Add(Player, Display);
}

void ADRLobbyGameMode::UpdateDisplayCharacter(AController* Player, EPlayerCharacterClass NewClass)
{
	DestroyDisplayCharacter(Player);

	int32* SlotIdx = PlayerSlotMap.Find(Player);
	if (SlotIdx)
	{
		SpawnDisplayCharacter(Player, NewClass, *SlotIdx);
	}
}

void ADRLobbyGameMode::DestroyDisplayCharacter(AController* Player)
{
	TWeakObjectPtr<ADRCharacter>* Found = DisplayCharacterMap.Find(Player);
	if (Found && Found->IsValid())
	{
		Found->Get()->Destroy();
	}
	DisplayCharacterMap.Remove(Player);
}

void ADRLobbyGameMode::DestroyAllDisplayCharacters()
{
	for (auto& Pair : DisplayCharacterMap)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value.Get()->Destroy();
		}
	}
	DisplayCharacterMap.Empty();
}

void ADRLobbyGameMode::ExecuteTravel(const FString& StageMapName)
{
	if (!HasAuthority()) return;

	// 留??꾪솚 ??罹먮┃???좏깮 ?뺣낫瑜?GameInstance?????(留??꾪솚 諛⑹떇??愿怨꾩뾾??蹂댁〈)
	if (UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance()))
	{
		GI->SaveAllPlayerSelections(GetWorld());
	}

	// 留??꾪솚 ???뺣━ ?묒뾽 (遺紐??대옒?ㅼ쓽 怨듯넻 ?⑥닔 ?ъ슜)
	PrepareForTravel();

	UWorld* World = GetWorld();
	if (World)
	{
		bUseSeamlessTravel = true;
		// URL 援ъ꽦
		FString TravelURL = StageMapName;
		if (!TravelURL.Contains(TEXT("?")))
		{
			TravelURL += TEXT("?listen");
		}

		// 留??대룞
		World->ServerTravel(TravelURL);
	}
}


