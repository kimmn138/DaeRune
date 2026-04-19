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

		// 대기실 상태일 때 슬롯 배치 + 디스플레이 캐릭터 스폰
		if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
		{
			FTimerHandle SlotTimerHandle;
			GetWorldTimerManager().SetTimer(
				SlotTimerHandle,
				[this, DRPC]()
				{
					if (!IsValid(DRPC)) return;
					AssignPlayerToSlot(DRPC);

					// 디스플레이 캐릭터 스폰
					ADRPlayerState* PS = DRPC->GetPlayerState<ADRPlayerState>();
					int32* SlotIdx = PlayerSlotMap.Find(DRPC);
					if (PS && SlotIdx)
					{
						SpawnDisplayCharacter(DRPC, PS->GetSelectedPlayerClass(), *SlotIdx);
					}

					// 고정 카메라로 ViewTarget 설정
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
			// FreeRoam 상태: 기존 복원 로직
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
			GEngine->AddOnScreenDebugMessage(
				1,
				600.f,
				FColor::Yellow,
				FString::Printf(TEXT("Players in game: %d"), NumberOfPlayers)
			);

			APlayerState* PlayerState = NewPlayer->GetPlayerState<APlayerState>();
			if (PlayerState)
			{
				FString PlayerName = PlayerState->GetPlayerName();
				GEngine->AddOnScreenDebugMessage(
					-1,
					60.f,
					FColor::Cyan,
					FString::Printf(TEXT("%s has joined the game!"), *PlayerName)
				);
			}
		}
	}
}

void ADRLobbyGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);

	UE_LOG(LogTemp, Log, TEXT("HandleSeamlessTravelPlayer called for %s"), *C->GetName());

	if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(C))
	{
		ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();

		FTimerHandle RestoreTimerHandle;
		GetWorldTimerManager().SetTimer(
			RestoreTimerHandle,
			[this, DRPC, LGS]()
			{
				if (!IsValid(DRPC)) return;

				// 관전 모드 강제 종료
				DRPC->ClientStopSpectating();

				if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
				{
					// SeamlessTravel로 가져온 Pawn 정리 (대기실에서는 디스플레이 사용)
					if (APawn* TravelPawn = DRPC->GetPawn())
					{
						DRPC->UnPossess();
						TravelPawn->Destroy();
					}

					// 대기실 모드: 슬롯 배치 + 디스플레이 캐릭터 + 고정 카메라
					AssignPlayerToSlot(DRPC);

					ADRPlayerState* PS = DRPC->GetPlayerState<ADRPlayerState>();
					int32* SlotIdx = PlayerSlotMap.Find(DRPC);
					if (PS && SlotIdx)
					{
						SpawnDisplayCharacter(DRPC, PS->GetSelectedPlayerClass(), *SlotIdx);
					}

					if (WaitingRoomCamera)
					{
						DRPC->ClientSetWaitingRoomView(WaitingRoomCamera);
					}
				}
				else
				{
					// 자유 조작 모드: 기존 복원 로직
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
	// 디스플레이 캐릭터 제거
	DestroyDisplayCharacter(Exiting);

	// 슬롯 매핑에서 제거
	if (PlayerSlotMap.Contains(Exiting))
	{
		PlayerSlotMap.Remove(Exiting);

		// 대기실 상태면 남은 플레이어 재배치
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
			GEngine->AddOnScreenDebugMessage(
				1,
				600.f,
				FColor::Yellow,
				FString::Printf(TEXT("Players in game: %d"), NumberOfPlayers - 1)
			);

			FString PlayerName = PlayerState->GetPlayerName();
			GEngine->AddOnScreenDebugMessage(
				-1,
				60.f,
				FColor::Cyan,
				FString::Printf(TEXT("%s has exited the game!"), *PlayerName)
			);
		}
	}
}

void ADRLobbyGameMode::TravelToStage(const FString& StageMapName, ADRPlayerController* Requester)
{
	// ���� üũ
	if (!HasAuthority()) return;

	// ȣ��Ʈ ���� üũ
	if (!Requester || !Requester->IsLocalController()) return;

	// �� �̸� ��ȿ��
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

	// TODO: ���� UI ǥ�� (�κ�� ������ UI)

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
		// �κ񿡼��� ���� ���� ���!
		SessionsSubsystem->UpdateSessionJoinability(true);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
				TEXT("Lobby loaded - Join in progress ALLOWED!"));
		}
	}
}

void ADRLobbyGameMode::RestartLobby()
{
	if (!HasAuthority()) return;

	UWorld* World = GetWorld();
	if (World)
	{
		FString CurrentMapName = World->GetMapName();

		// PIE(Play In Editor) �����Ƚ� ����
		// PIE������ "UEDPIE_0_MapName" �������� ����
		CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);

		bUseSeamlessTravel = true;

		// ���� �� �����
		World->ServerTravel(CurrentMapName + TEXT("?listen"));
	}

	// �÷��� ����
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
		if (!PlayerCharacterClassInfo) continue;
		TSubclassOf<ADRCharacter>* BPClassPtr = PlayerCharacterClassInfo->CharacterBPClasses.Find(PS->GetSelectedPlayerClass());
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
	GetWorldTimerManager().SetTimer(
		TransitionTimerHandle,
		[LGS]()
		{
			if (IsValid(LGS))
			{
				LGS->SetLobbyState(ELobbyState::FreeRoam);
			}
		},
		2.0f, // 카메라 전환 시간
		false
	);
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

	UE_LOG(LogTemp, Log, TEXT("FindWaitingRoomActors: Camera=%s, Slots=%d"),
		WaitingRoomCamera ? *WaitingRoomCamera->GetName() : TEXT("NULL"),
		WaitingRoomSlots.Num());

	if (WaitingRoomSlots.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindWaitingRoomActors: No WaitingRoomSlot TargetPoints found! "
			"Ensure TargetPoints have tags starting with 'WaitingRoomSlot'."));
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
	if (!PlayerCharacterClassInfo) return;
	TSubclassOf<ADRCharacter>* BPClassPtr = PlayerCharacterClassInfo->CharacterBPClasses.Find(NewClass);
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

	// 어빌리티 완전 클리어
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

void ADRLobbyGameMode::SpawnDisplayCharacter(AController* Player, EPlayerCharacterClass CharClass, int32 SlotIndex)
{
	if (!PlayerCharacterClassInfo || !Player) return;
	TSubclassOf<ADRCharacter>* BPClassPtr = PlayerCharacterClassInfo->CharacterBPClasses.Find(CharClass);
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

