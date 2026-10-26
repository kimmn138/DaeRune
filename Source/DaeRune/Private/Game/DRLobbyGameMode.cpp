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
#include "DaeRune/DRLogChannels.h"

ADRLobbyGameMode::ADRLobbyGameMode()
{
	WipeoutDelayTime = 2.0f;
}

namespace
{
	// 심리스 트래블 · 관전 등으로 멈춰 있던 폰의 이동 · 충돌을 되살린다
	void RestorePawnMovement(APawn* Pawn)
	{
		if (!Pawn) return;

		if (UCharacterMovementComponent* MovementComp = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()))
		{
			MovementComp->SetMovementMode(MOVE_Walking);
			MovementComp->SetComponentTickEnabled(true);
		}

		if (UCapsuleComponent* CapsuleComp = Cast<UCapsuleComponent>(Pawn->GetRootComponent()))
		{
			CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}

	// 접속 직후 처리를 잠시 미룬다 (클라 초기화 대기). ★게임모드 · PC 둘 다 약참조★ — 그 사이 나가거나 맵이 바뀌어도 안전
	template <typename FuncType>
	void DeferForPlayer(ADRLobbyGameMode* GameMode, ADRPlayerController* DRPC, float Delay, FuncType&& Func)
	{
		TWeakObjectPtr<ADRPlayerController> WeakPC(DRPC);
		FTimerHandle Handle;
		GameMode->GetWorldTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateWeakLambda(GameMode, [WeakPC, Func = Forward<FuncType>(Func)]()
			{
				if (ADRPlayerController* PC = WeakPC.Get())
				{
					Func(PC);
				}
			}),
			Delay, false);
	}
}

void ADRLobbyGameMode::PlaceInWaitingRoom(ADRPlayerController* DRPC, EPlayerCharacterClass DisplayClass)
{
	// 슬롯 배치 + 디스플레이 캐릭터 + 고정 카메라
	AssignPlayerToSlot(DRPC);

	if (const int32* SlotIdx = PlayerSlotMap.Find(DRPC))
	{
		SpawnDisplayCharacter(DRPC, DisplayClass, *SlotIdx);
	}

	if (WaitingRoomCamera)
	{
		DRPC->ClientSetWaitingRoomView(WaitingRoomCamera);
	}
}

void ADRLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ADRPlayerController* DRPC = Cast<ADRPlayerController>(NewPlayer);
	if (!DRPC) return;

	const ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
	if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
	{
		// 대기실: 슬롯 배치 + 디스플레이 캐릭터 + 고정 카메라
		DeferForPlayer(this, DRPC, 0.5f, [this](ADRPlayerController* PC)
		{
			const ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>();
			PlaceInWaitingRoom(PC, PS ? PS->GetSelectedPlayerClass() : EPlayerCharacterClass::Gardener);
		});
	}
	else
	{
		// FreeRoam: 관전 해제 + 이동 · 충돌 복원
		DeferForPlayer(this, DRPC, 0.5f, [](ADRPlayerController* PC)
		{
			PC->ClientStopSpectating();
			RestorePawnMovement(PC->GetPawn());
		});
	}
}

void ADRLobbyGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	ADRPlayerController* DRPC = Cast<ADRPlayerController>(C);
	if (!DRPC) return;

	DeferForPlayer(this, DRPC, 0.5f, [this](ADRPlayerController* PC)
	{
		// 관전 모드 강제 종료
		PC->ClientStopSpectating();

		const ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
		if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
		{
			// SeamlessTravel로 가져온 Pawn 정리 (대기실에서는 디스플레이 사용)
			if (APawn* TravelPawn = PC->GetPawn())
			{
				PC->UnPossess();
				TravelPawn->Destroy();
			}

			// PlayerController 캐시에서 선택 클래스 복원 (Seamless Travel 시 PlayerState 유실 방지)
			const EPlayerCharacterClass CachedClass = PC->GetCachedSelectedClass();
			if (ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>())
			{
				if (PS->GetSelectedPlayerClass() != CachedClass)
				{
					PS->SetSelectedPlayerClass(CachedClass);
				}
			}
			PlaceInWaitingRoom(PC, CachedClass);
		}
		else
		{
			// 자유 조작 모드: 이동 · 충돌 복원
			RestorePawnMovement(PC->GetPawn());
		}
	});
}

void ADRLobbyGameMode::Logout(AController* Exiting)
{
	// 디스플레이 캐릭터 제거
	DestroyDisplayCharacter(Exiting);

	// 슬롯 매핑에서 제거
	if (PlayerSlotMap.Remove(Exiting) > 0)
	{
		// 대기실 상태면 남은 플레이어 재배치
		ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
		if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
		{
			RepositionAllPlayers();
		}
	}

	Super::Logout(Exiting);
}

void ADRLobbyGameMode::TravelToStage(const FString& StageMapName, ADRPlayerController* Requester)
{
	// 서버 체크
	if (!HasAuthority()) return;

	// 호스트 권한 체크
	if (!Requester || !Requester->IsLocalController()) return;

	// 맵 이름 유효성
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

	// TODO: 전멸 UI 표시 (로비는 가벼운 UI)

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
		// 로비에서는 도중 참가 허용!
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

		// PIE(Play In Editor) 프리픽스 제거
		// PIE에서는 "UEDPIE_0_MapName" 형식으로 나옴
		CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);

		bUseSeamlessTravel = true;

		// 같은 맵 재시작
		World->ServerTravel(CurrentMapName + TEXT("?listen"));
	}

	// 플래그 리셋
	bIsWipeoutInProgress = false;
}

void ADRLobbyGameMode::PowerOn(ADRPlayerController* Requester)
{
	if (!HasAuthority()) return;

	// 호스트 권한 체크
	if (!Requester || !Requester->IsLocalController()) return;

	ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
	if (!LGS) return;

	// 대기실 상태에서만 가능
	if (LGS->GetLobbyState() != ELobbyState::WaitingRoom) return;

	// 모든 비호스트 플레이어가 준비 상태여야 PowerOn 가능
	if (!LGS->AreAllNonHostPlayersReady()) return;

	// 1. 세션 참가 차단
	BlockJoinInProgress();

	// 2. 전환 상태로 변경
	LGS->SetLobbyState(ELobbyState::Transitioning);

	// 3. 디스플레이 캐릭터 제거
	DestroyAllDisplayCharacters();

	// 4. 모든 플레이어에게 진짜 캐릭터 스폰 + Possess
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get());
		if (!PC) continue;

		ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>();
		if (!PS) continue;

		// 선택된 클래스의 BP 스폰
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

		// Possess (최초 1회, 깜빡임 없음)
		PC->Possess(NewPawn);

		// 이동 활성화
		NewPawn->SetReplicateMovement(true);
		if (UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(NewPawn->GetMovementComponent()))
		{
			CMC->SetMovementMode(MOVE_Walking);
		}

		// 카메라 전환 명령 (Client RPC)
		PC->ClientStartCameraTransitionToCharacter();
	}

	// 5. 전환 완료 후 FreeRoam으로 상태 변경 (카메라 연출 시간만큼 딜레이)
	FTimerHandle TransitionTimerHandle;
	TWeakObjectPtr<ADRLobbyGameState> WeakLGS(LGS);
	GetWorldTimerManager().SetTimer(
		TransitionTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [WeakLGS]()
		{
			if (ADRLobbyGameState* State = WeakLGS.Get())
			{
				State->SetLobbyState(ELobbyState::FreeRoam);
			}
		}),
		2.0f, // 카메라 전환 시간
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

	// 호스트 권한 체크
	if (!Requester || !Requester->IsLocalController()) return;

	// 타겟 유효성 체크
	if (!TargetPlayer) return;

	// 자기 자신은 킥 불가
	if (Requester == TargetPlayer) return;

	// 대기실 상태에서만 가능
	ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
	if (!LGS || LGS->GetLobbyState() != ELobbyState::WaitingRoom) return;

	// 디스플레이 캐릭터 + 슬롯에서 제거
	DestroyDisplayCharacter(TargetPlayer);
	PlayerSlotMap.Remove(TargetPlayer);

	// 클라이언트에게 킥 알림 → 메인 메뉴로 이동
	TargetPlayer->ClientKicked(TEXT("You have been kicked by the host."));

	// 나머지 플레이어 재배치 (내부에서 BroadcastRefreshWaitingRoomUI 호출)
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
	// 카메라 탐색
	for (TActorIterator<ADRWaitingRoomCameraActor> It(GetWorld()); It; ++It)
	{
		WaitingRoomCamera = *It;
		break; // 하나만 필요
	}

	// "WaitingRoomSlot" 태그가 있는 ATargetPoint 탐색
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

	// 태그 suffix 숫자 기준 정렬 (WaitingRoomSlot_0, WaitingRoomSlot_1, ...)
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
		UE_LOG(LogDR, Warning, TEXT("[Lobby] 대기실 슬롯(WaitingRoomSlot)이 레벨에 없습니다."));
	}
}

void ADRLobbyGameMode::AssignPlayerToSlot(AController* Player)
{
	if (WaitingRoomSlots.Num() == 0) return;

	int32 SlotIndex = NextAvailableSlot++;
	PlayerSlotMap.Add(Player, SlotIndex);

	// PlayerState에 권위적 슬롯 인덱스 설정 (클라이언트에 복제됨)
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

	// 슬롯 인덱스 클램프 (범위 초과 시 마지막 슬롯 사용)
	int32 ClampedIndex = FMath::Clamp(SlotIndex, 0, WaitingRoomSlots.Num() - 1);
	AActor* SlotActor = WaitingRoomSlots[ClampedIndex];
	if (!SlotActor) return;

	// 카메라를 향하는 회전 계산
	FRotator FacingRotation = FRotator::ZeroRotator;
	if (WaitingRoomCamera)
	{
		FacingRotation = WaitingRoomCamera->GetCharacterFacingRotation();
	}

	FVector SlotLocation = SlotActor->GetActorLocation();

	// CMC 위치 리플리케이션 비활성화 (unreliable 위치 보정이 Multicast 결과를 덮어쓰는 것 방지)
	Pawn->SetReplicateMovement(false);

	// Multicast RPC로 모든 엔드포인트에서 직접 위치 설정 (CMC 우회)
	if (ADRCharacter* DRChar = Cast<ADRCharacter>(Pawn))
	{
		DRChar->MulticastTeleportToSlot(SlotLocation, FacingRotation);
	}

	// CMC 비활성화 (서버에서만 — 이동 입력 차단용)
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

	// 기존 매핑 수집 (호스트 우선)
	TArray<AController*> OrderedPlayers;

	// 호스트를 먼저 찾기
	for (auto& Pair : PlayerSlotMap)
	{
		if (ADRPlayerController* PC = Cast<ADRPlayerController>(Pair.Key))
		{
			if (PC->IsLocalController() && HasAuthority())
			{
				OrderedPlayers.Insert(Pair.Key, 0); // 호스트를 맨 앞에
				continue;
			}
		}
		OrderedPlayers.Add(Pair.Key);
	}

	// 매핑 재구성
	PlayerSlotMap.Empty();
	NextAvailableSlot = 0;

	for (AController* Player : OrderedPlayers)
	{
		AssignPlayerToSlot(Player);
	}

	// 디스플레이 캐릭터 위치 갱신 (NetMulticast RPC로 모든 클라이언트에 전파)
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

				// 캡슐 반높이만큼 Z 보정 (TargetPoint가 바닥 레벨인 경우 바닥 뚫림 방지)
				if (UCapsuleComponent* Capsule = Display->GetCapsuleComponent())
				{
					Loc.Z += Capsule->GetScaledCapsuleHalfHeight();
				}

				// NetMulticast RPC로 모든 엔드포인트에서 위치 갱신
				Display->MulticastTeleportToSlot(Loc, Rot);
			}
		}
	}

	// 모든 클라이언트의 대기실 UI 갱신 (Client RPC 사용)
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

	// 대기실: 디스플레이만 교체 (Possess/UnPossess 없음 → 깜빡임 근본 제거)
	if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
	{
		UpdateDisplayCharacter(PC, NewClass);
		BroadcastRefreshWaitingRoomUI();
		return;
	}

	// FreeRoam: 기존 폰 교체 로직 (진짜 Possess 필요)
	UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(this);
	if (!ClassInfo) return;
	TSubclassOf<ADRCharacter>* BPClassPtr = ClassInfo->CharacterBPClasses.Find(NewClass);
	if (!BPClassPtr || !*BPClassPtr) return;

	// 어빌리티 완전 클리어 — 빙의 시점(PossessedBy)에 새 클래스의 시작 어빌리티를 다시 받는다
	if (ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>())
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			ASC->CancelAllAbilities();
			ASC->ClearAllAbilities();

			if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(ASC))
			{
				DRASC->bStartupAbilitiesGiven = false;
			}
		}
	}

	const APawn* OldPawn = PC->GetPawn();
	const FTransform SpawnTransform = OldPawn ? OldPawn->GetActorTransform() : FTransform::Identity;
	RespawnPlayerPawn(PC, *BPClassPtr, SpawnTransform);
}

UClass* ADRLobbyGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();

	// 대기실/전환 중에는 Pawn 스폰 억제 (디스플레이 캐릭터 사용)
	if (LGS && (LGS->GetLobbyState() == ELobbyState::WaitingRoom
			 || LGS->GetLobbyState() == ELobbyState::Transitioning))
	{
		return nullptr;
	}

	// FreeRoam: 기존 로직 (선택된 클래스의 BP 반환)
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

	// 슬롯 위치/회전 계산
	int32 ClampedIndex = FMath::Clamp(SlotIndex, 0, WaitingRoomSlots.Num() - 1);
	FVector Location = WaitingRoomSlots[ClampedIndex]->GetActorLocation();
	FRotator Rotation = WaitingRoomCamera
		? WaitingRoomCamera->GetCharacterFacingRotation()
		: FRotator::ZeroRotator;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ADRCharacter* Display = GetWorld()->SpawnActor<ADRCharacter>(*BPClassPtr, FTransform(Rotation, Location), Params);
	if (!Display) return;

	// 이동 비활성화 (디스플레이용)
	if (UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(Display->GetMovementComponent()))
	{
		CMC->StopMovementImmediately();
		CMC->DisableMovement();
	}

	// 리플리케이트 이동 비활성화
	Display->SetReplicateMovement(false);

	// 대기실에서도 입은 옷이 보이게 — 보고가 아직이면 빈 목록이고, 도착하면 RefreshDisplayCharacterSkin 이 다시 싣는다 (Plan(옷장) M5)
	if (const ADRPlayerState* PS = Player->GetPlayerState<ADRPlayerState>())
	{
		Display->SetDisplaySkinIds(PS->GetEquippedSkinIds());
	}

	DisplayCharacterMap.Add(Player, Display);
}

void ADRLobbyGameMode::RefreshDisplayCharacterSkin(AController* Player)
{
	if (!Player) return;

	const TWeakObjectPtr<ADRCharacter>* Found = DisplayCharacterMap.Find(Player);
	if (!Found || !Found->IsValid()) return;

	if (const ADRPlayerState* PS = Player->GetPlayerState<ADRPlayerState>())
	{
		Found->Get()->SetDisplaySkinIds(PS->GetEquippedSkinIds());
	}
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

	// 맵 전환 전 캐릭터 선택 정보를 GameInstance 에 저장 (맵 전환 방식과 관계없이 보존)
	if (UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance()))
	{
		GI->SaveAllPlayerSelections(GetWorld());
	}

	// 맵 전환 전 정리 작업 (부모 클래스의 공통 함수 사용)
	PrepareForTravel();

	UWorld* World = GetWorld();
	if (World)
	{
		bUseSeamlessTravel = true;
		// URL 구성
		FString TravelURL = StageMapName;
		if (!TravelURL.Contains(TEXT("?")))
		{
			TravelURL += TEXT("?listen");
		}

		// 맵 이동
		World->ServerTravel(TravelURL);
	}
}


