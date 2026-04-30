// Copyright DaeRune


#include "Player/DRPlayerController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DRGameplayTags.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "Input/DRInputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UI/Widget/DamageTextComponent.h"
#include "Actor/DRCleanserPart.h"
#include "Actor/DRCleanserSite.h"
#include "Actor/DRWaitingRoomCameraActor.h"
#include "Character/DRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Game/DRLobbyGameState.h"
#include "UI/HUD/DRHUD.h"
#include "Player/DRPlayerState.h"
#include "UI/WidgetController/DRWidgetController.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "Game/DRSettingsManager.h"
#include "Game/DRGameUserSettings.h"
#include "Sound/DRSoundManager.h"
#include "Actor/DRBGMActor.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Widget/DRWaitingRoomWidget.h"
#include "Game/DRLobbyGameMode.h"
#include "MultiplayerSessionsSubsystem.h"

ADRPlayerController::ADRPlayerController()
{
	// ��Ƽ�÷��� ���ø����̼� Ȱ��ȭ
	bReplicates = true;
	
	// ��ǰ �ý��� �ʱ�ȭ
	bPartDetectionEnabled = false;
	CurrentDetectedPart = nullptr;
	NearbyPart = nullptr;
	LineTraceTimer = 0.f;
}

void ADRPlayerController::CorruptedStateChanged(bool bIsStateChanged)
{
	// ���� ���� �÷��� ������Ʈ
	bIsCorrupted = bIsStateChanged;

	// ���� ä�� ����
	//SetVoiceChatEnabled(!bIsCorrupted);

	// �� ���� �ð� ȿ�� ����
	//SetTeamVisualsEnabled(!bIsCorrupted);
}

void ADRPlayerController::UpdateVoiceChannelForDeathState(bool bIsDead)
{
	// 로컬 컨트롤러에서만 실행
	if (!IsLocalController()) return;

	bIsDeadForVoice = bIsDead;

	// 모든 플레이어에 대한 뮤트 상태 업데이트
	RefreshAllPlayerVoiceMutes();
}

void ADRPlayerController::SetPlayerVoiceMuted(APlayerState* TargetPlayer, bool bMute)
{
	if (!TargetPlayer || !IsLocalController()) return;

	// 자기 자신은 뮤트하지 않음
	if (TargetPlayer == PlayerState) return;

	// PlayerController의 내장 뮤트 함수 사용
	FUniqueNetIdRepl TargetNetId = TargetPlayer->GetUniqueId();
	if (TargetNetId.IsValid())
	{
		if (bMute)
		{
			// 뮤트 리스트에 추가
			GameplayMutePlayer(TargetNetId);
		}
		else
		{
			// 뮤트 리스트에서 제거
			GameplayUnmutePlayer(TargetNetId);
		}
	}
}

void ADRPlayerController::RefreshAllPlayerVoiceMutes()
{
	if (!IsLocalController()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	AGameStateBase* GameState = World->GetGameState();
	if (!GameState) return;

	// 모든 플레이어 순회
	for (APlayerState* OtherPS : GameState->PlayerArray)
	{
		if (!OtherPS || OtherPS == PlayerState) continue;

		// 상대방의 사망 상태 확인
		bool bOtherIsDead = false;

		if (APawn* OtherPawn = OtherPS->GetPawn())
		{
			if (OtherPawn->Implements<UCombatInterface>())
			{
				bOtherIsDead = ICombatInterface::Execute_IsDead(OtherPawn);
			}
		}
		else
		{
			// Pawn이 없으면 죽은 것으로 간주
			bOtherIsDead = true;
		}

		bool bShouldMute = false;

		if (!bIsDeadForVoice)
		{
			// 내가 살아있으면, 죽은 플레이어는 뮤트
			bShouldMute = bOtherIsDead;
		}
		else
		{
			// 내가 죽었으면, 모두 들림
			bShouldMute = false;
		}

		SetPlayerVoiceMuted(OtherPS, bShouldMute);
	}
}

void ADRPlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter)
{
	// ���� ��Ʈ�ѷ������� ������ �ؽ�Ʈ ǥ��
	if (IsValid(TargetCharacter) && DamageTextComponentClass && IsLocalController())
	{
		// ������ �ؽ�Ʈ ������Ʈ ���� �� ����
		UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);
		DamageText->RegisterComponent();

		// Ÿ�� ĳ���Ϳ� �Ͻ������� ���� �� �и� (���� ��ġ ����)
		DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

		// ������ ��ġ ���� �� �ִϸ��̼� ����
		DamageText->SetDamageText(DamageAmount);
	}
}

void ADRPlayerController::SetPartDetectionEnabled(bool bEnabled, ADRCleanserPart* Part)
{
	if (bEnabled)
	{
		// ����Ʈ���̽� Ȱ��ȭ
		bPartDetectionEnabled = true;
		NearbyPart = Part;
		LineTraceTimer = 0.f;
	}
	else
	{
		// �ش� ��ǰ�� ���� ��ó ��ǰ�� ���� ���� ��Ȱ��ȭ
		if (NearbyPart == Part)
		{
			bPartDetectionEnabled = false;
			NearbyPart = nullptr;
			
			// ���� ������ ��ǰ�� ������ UI ���� �˸�
			if (CurrentDetectedPart)
			{
				ServerNotifyLineTraceLost(CurrentDetectedPart);
				CurrentDetectedPart = nullptr;
			}
		}
	}
}

ADRCleanserPart* ADRPlayerController::FindPartByLineTrace()
{
	// ĳ���� ��������
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return nullptr;

	// ĳ���Ͱ� �̹� ��ǰ�� ��� ������ �������� ����
	if (DRCharacter->IsCarryingPart()) return nullptr;

	// ī�޶� ������Ʈ ��������
	UCameraComponent* Camera = DRCharacter->FindComponentByClass<UCameraComponent>();
	if (!Camera) return nullptr;

	// ����Ʈ���̽� ����/�� ��ġ ���
	FVector Start = Camera->GetComponentLocation();
	FVector End = Start + Camera->GetForwardVector() * LineTraceDistance;

	// ����Ʈ���̽� ����
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(DRCharacter);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);

	// ��ǰ�� ��Ʈ�ߴ��� Ȯ��
	if (bHit)
	{
		ADRCleanserPart* HitPart = Cast<ADRCleanserPart>(HitResult.GetActor());
		if (HitPart && HitPart->CanBePickedUp())
		{
			return HitPart;
		}
	}

	return nullptr;
}

void ADRPlayerController::ClientShowPartPickupUI_Implementation()
{
	OnPartPickedUp();
}

void ADRPlayerController::ServerNotifyLineTraceDetected_Implementation(ADRCleanserPart* Part)
{
	if (!Part) return;

	// 서버에서 처리 (UI 갱신은 멀티캐스트로)
	Part->MulticastShowInteractionUI(this, true);
}

void ADRPlayerController::ServerNotifyLineTraceLost_Implementation(ADRCleanserPart* Part)
{
	if (!Part) return;

	Part->MulticastShowInteractionUI(this, false);
}

void ADRPlayerController::ServerRequestInstallPartToSite_Implementation(ADRCleanserSite* Site)
{
	if (!Site) return;
	// 캐릭터 가져오기
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// 클렌저 사이트에 부품 설치
	Site->InstallPart(DRCharacter);
}

void ADRPlayerController::ClientStartSpectating_Implementation()
{
	if (!IsLocalController()) return;
    
	bIsSpectating = true;
	CurrentSpectatedPlayerIndex = 0;

	ADRGameStateBase* GameStateBase = GetWorld()->GetGameState<ADRGameStateBase>();
	if (!GameStateBase) return;

	TArray<ADRCharacter*> AlivePlayers = GameStateBase->GetAlivePlayers();
	
	if (AlivePlayers.Num() > 0)
	{
		SetSpectateTarget(AlivePlayers[0]);
	}
}

void ADRPlayerController::ClientStopSpectating_Implementation()
{
	if (!IsLocalController()) return;

	UE_LOG(LogTemp, Log, TEXT("ClientStopSpectating_Implementation - Pawn: %s"), GetPawn() ? *GetPawn()->GetName() : TEXT("NULL"));

	// 관전 상태 강제 초기화 (bIsSpectating 여부와 상관없이)
	bIsSpectating = false;
	CurrentSpectatedPlayerIndex = 0;

	// 델리게이트 해제
	if (CurrentSpectatedCharacter.IsValid())
	{
		if (ADRCharacterBase* OldTarget = Cast<ADRCharacterBase>(CurrentSpectatedCharacter.Get()))
		{
			OldTarget->OnDeathDelegate.RemoveDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
		}
	}
	CurrentSpectatedCharacter.Reset();

	// 대기실이면 ViewTarget 복원 스킵 (ClientSetWaitingRoomView가 담당)
	if (bIsInWaitingRoom)
	{
		// FreeRoam에서 ClientStopSpectating이 호출된 경우 대기실 플래그 교정
		ADRLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<ADRLobbyGameState>() : nullptr;
		if (LGS && LGS->GetLobbyState() == ELobbyState::FreeRoam)
		{
			bIsInWaitingRoom = false;
			// 아래로 계속 진행하여 ViewTarget 복원
		}
		else
		{
			return;
		}
	}

	// ViewTarget 복원
	if (APawn* MyPawn = GetPawn())
	{
		SetViewTarget(MyPawn);
		RestoreDefaultInputMode();
	}
	else
	{
		// Pawn이 아직 없으면 딜레이 후 재시도
		UE_LOG(LogTemp, Warning, TEXT("ClientStopSpectating - No Pawn yet, retrying in 0.5s"));

		if (UWorld* World = GetWorld())
		{
			FTimerHandle RetryTimer;
			World->GetTimerManager().SetTimer(
				RetryTimer,
				[WeakThis = TWeakObjectPtr<ADRPlayerController>(this)]()
				{
					if (ADRPlayerController* PC = WeakThis.Get())
					{
						// 대기실이면 ViewTarget 복원 스킵
						if (PC->bIsInWaitingRoom) return;

						if (APawn* MyPawn = PC->GetPawn())
						{
							PC->SetViewTarget(MyPawn);
							UE_LOG(LogTemp, Log, TEXT("ClientStopSpectating - Pawn found on retry: %s"), *MyPawn->GetName());
						}
						PC->RestoreDefaultInputMode();
					}
				},
				0.5f,
				false
			);
		}
	}
}

void ADRPlayerController::SpectateNextPlayer()
{
	if (!bIsSpectating || !IsLocalController()) return;

	ADRGameStateBase* GameStateBase = GetWorld()->GetGameState<ADRGameStateBase>();
	if (!GameStateBase) return;

	TArray<ADRCharacter*> AliveCharacters = GameStateBase->GetAlivePlayers();
	if (AliveCharacters.Num() == 0)
	{
		// 모두 사망 - 관전 대상 없음
		CurrentSpectatedCharacter.Reset();
		return;
	}
	
	if(AliveCharacters.Num() == 1) return;

	// 다음 인덱스 계산
	CurrentSpectatedPlayerIndex = (CurrentSpectatedPlayerIndex + 1) % AliveCharacters.Num();
    
	// 새 관전 대상 설정
	SetSpectateTarget(AliveCharacters[CurrentSpectatedPlayerIndex]);
}

void ADRPlayerController::SpectatePreviousPlayer()
{
	if (!bIsSpectating || !IsLocalController()) return;

	ADRGameStateBase* GameStateBase = GetWorld()->GetGameState<ADRGameStateBase>();
	if (!GameStateBase) return;

	TArray<ADRCharacter*> AliveCharacters = GameStateBase->GetAlivePlayers();
	if (AliveCharacters.Num() == 0)
	{
		CurrentSpectatedCharacter.Reset();
		return;
	}

	if(AliveCharacters.Num() == 1) return;

	// 이전 인덱스 계산
	CurrentSpectatedPlayerIndex--;
	if (CurrentSpectatedPlayerIndex < 0)
	{
		CurrentSpectatedPlayerIndex = AliveCharacters.Num() - 1;
	}

	// 새 관전 대상 설정
	SetSpectateTarget(AliveCharacters[CurrentSpectatedPlayerIndex]);
}

void ADRPlayerController::ClientStopAllAudio_Implementation()
{
	// BGM 정지
	TArray<AActor*> BGMActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADRBGMActor::StaticClass(), BGMActors);
	for (AActor* Actor : BGMActors)
	{
		if (ADRBGMActor* BGMActor = Cast<ADRBGMActor>(Actor))
		{
			BGMActor->StopBGM(0.0f);
		}
	}

	// VOIP 관련 SynthComponent 정리 (SeamlessTravel 전 필수)
	// DestroyComponent() 직접 호출 시 렌더 씬에서 AudioComponent가 해제되지 않아 크래시 발생
	// 안전한 정리 순서: Deactivate -> UnregisterComponent
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);

	for (AActor* Actor : AllActors)
	{
		if (!Actor) continue;

		TArray<UActorComponent*> AllComps;
		Actor->GetComponents<UActorComponent>(AllComps);

		for (UActorComponent* Comp : AllComps)
		{
			if (Comp && Comp->GetClass()->GetName().Contains(TEXT("VoipListenerSynthComponent")))
			{
				// 1. 먼저 비활성화
				Comp->Deactivate();

				// 2. 씬에서 등록 해제
				if (Comp->IsRegistered())
				{
					Comp->UnregisterComponent();
				}
			}
		}
	}
}

void ADRPlayerController::CheatSkipToNextPhase()
{
// 개발 빌드에서만 동작하도록 체크
#if !UE_BUILD_SHIPPING
	ServerCheatSkipToNextPhase();
#else
	UE_LOG(LogTemp, Warning, TEXT("CheatSkipToNextPhase: Shipping 빌드에서는 사용할 수 없습니다."));
#endif
}

void ADRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Enhanced Input Context�� �����Ǿ� �ִ��� Ȯ��
	check(DRContext);

	// Enhanced Input ����ý��ۿ� ���� ���ؽ�Ʈ �߰�
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->AddMappingContext(DRContext, 0);
	}

	// UI ����
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());

	// �÷��̾�� TeamId 0
	SetGenericTeamId(FGenericTeamId(0));

	// 카메라 피치 제한 설정
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -ViewPitchMin;  // 아래 (음수)
		PlayerCameraManager->ViewPitchMax = ViewPitchMax;   // 위 (양수)
	}

	// 로컬 플레이어만 오디오 설정 적용
	if (IsLocalController())
	{
		// Manager 통해서 오디오 설정 적용
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDRSettingsManager* Manager = GI->GetSubsystem<UDRSettingsManager>())
			{
				Manager->ApplyAudioSettings();
			}
		}
	}

	RestoreDefaultInputMode();
}

void ADRPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (bIsSpectating) return;
	
	// ��ǰ ������ ��Ȱ��ȭ�Ǿ� ������ ��ŵ
	if (!bPartDetectionEnabled) return;

	// 로컬 컨트롤러에서만 라인트레이싱 실행
	if (!IsLocalController()) return;

	// ����Ʈ���̽� Ÿ�̸� ������Ʈ
	LineTraceTimer += DeltaTime;
	if (LineTraceTimer >= LineTraceUpdateInterval)
	{
		LineTraceTimer = 0.f;

		// ��ǰ ����
		ADRCleanserPart* DetectedPart = FindPartByLineTrace();

		// ������ ��ǰ�� ����Ǿ����� Ȯ��
		if (DetectedPart != CurrentDetectedPart)
		{
			// ������ ������ ��ǰ�� ������ �˸�
			if (CurrentDetectedPart)
			{
				ServerNotifyLineTraceLost(CurrentDetectedPart);
			}

			CurrentDetectedPart = DetectedPart;

			// ���� ������ ��ǰ�� ������ �˸�
			if (CurrentDetectedPart)
			{
				ServerNotifyLineTraceDetected(CurrentDetectedPart);
			}
		}
	}
}

void ADRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// DRInputComponent�� ĳ����
	UDRInputComponent* DRInputComponent = CastChecked<UDRInputComponent>(InputComponent);
	// �⺻ �Է� �׼� ���ε�
	DRInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Move);
	DRInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Look);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ADRPlayerController::StartJump);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ADRPlayerController::StopJump);
	DRInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &ADRPlayerController::HandleInteract);
	DRInputComponent->BindAction(SpectateNextAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleSpectateNext);
	DRInputComponent->BindAction(SpectatePreviousAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleSpectatePrevious);
	DRInputComponent->BindAction(ToggleSettingsAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleToggleSettings);
	// �����Ƽ �Է� ���ε� (InputConfig ���)
	DRInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}

void ADRPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	// 로컬 컨트롤러만 처리
	if (!IsLocalController()) return;

	UE_LOG(LogTemp, Log, TEXT("ADRPlayerController::ReceivedPlayer called"));

	// 레벨 진입 시 공통 초기화 수행
	OnLevelEntered();
}

void ADRPlayerController::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	// 로컬 컨트롤러만 처리
	if (!IsLocalController()) return;

	UE_LOG(LogTemp, Log, TEXT("ADRPlayerController::PostSeamlessTravel called"));

	// SeamlessTravel 후 레벨 진입 시 공통 초기화 수행
	OnLevelEntered();
}

void ADRPlayerController::OnLevelEntered()
{
	UE_LOG(LogTemp, Log, TEXT("ADRPlayerController::OnLevelEntered called - Level: %s"), *GetWorld()->GetMapName());

	// 현재 레벨 확인
	UWorld* World = GetWorld();
	if (!World) return;

	// 게임오버 UI가 남아있으면 제거
	if (CurrentResultWidget)
	{
		CurrentResultWidget->RemoveFromParent();
		CurrentResultWidget = nullptr;
	}

	// 대기실 위젯 정리 (레벨 이동 시 무효화된 포인터 정리)
	DestroyWaitingRoomUI();

	// 설정 메뉴 상태 초기화 (레벨 이동 시)
	if (bIsSettingsMenuOpen)
	{
		CloseSettingsMenu();
	}
	bIsSettingsMenuOpen = false;

	// 관전 상태 초기화 (델리게이트 정리 포함)
	if (CurrentSpectatedCharacter.IsValid())
	{
		if (ADRCharacterBase* OldTarget = Cast<ADRCharacterBase>(CurrentSpectatedCharacter.Get()))
		{
			OldTarget->OnDeathDelegate.RemoveDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
		}
	}
	bIsSpectating = false;
	CurrentSpectatedCharacter = nullptr;
	CurrentSpectatedPlayerIndex = 0;

	// 대기실 감지 → ViewTarget 복원 차단
	ADRLobbyGameState* LGS = World->GetGameState<ADRLobbyGameState>();

	if (IsInLobby())
	{
		if (!LGS)
		{
			// GameState 아직 복제 안됨 → 대기실로 가정 (서버 RPC가 이후 교정)
			UE_LOG(LogTemp, Log, TEXT("OnLevelEntered: Lobby detected but LGS not replicated yet, defaulting to WaitingRoom"));
			bIsInWaitingRoom = true;
			bAutoManageActiveCameraTarget = false; // Pawn 수신 시 ViewTarget 자동 설정 차단
			RestoreDefaultInputMode();
			// WaitingRoom UI는 ClientSetWaitingRoomView RPC에서 생성
			return;
		}

		if (LGS->GetLobbyState() == ELobbyState::WaitingRoom
			|| LGS->GetLobbyState() == ELobbyState::Transitioning)
		{
			bIsInWaitingRoom = true;
			bAutoManageActiveCameraTarget = false; // Pawn 수신 시 ViewTarget 자동 설정 차단
			RestoreDefaultInputMode();
			CreateWaitingRoomUI();
			return;  // ViewTarget은 ClientSetWaitingRoomView RPC가 설정
		}
	}

	bIsInWaitingRoom = false;

	// ViewTarget 즉시 복원 (잘못된 타겟 참조 방지)
	if (APawn* MyPawn = GetPawn())
	{
		SetViewTarget(MyPawn);
	}
	else
	{
		// Pawn이 아직 없으면 자기 자신으로 설정 후 딜레이 재시도
		SetViewTarget(this);

		FTimerHandle ViewTargetRetryTimer;
		World->GetTimerManager().SetTimer(
			ViewTargetRetryTimer,
			[WeakThis = TWeakObjectPtr<ADRPlayerController>(this)]()
			{
				if (ADRPlayerController* PC = WeakThis.Get())
				{
					// 대기실이면 ViewTarget 복원 스킵
					if (PC->bIsInWaitingRoom) return;

					if (APawn* MyPawn = PC->GetPawn())
					{
						PC->SetViewTarget(MyPawn);
						UE_LOG(LogTemp, Log, TEXT("ViewTarget restored to Pawn after delay"));
					}
				}
			},
			0.5f,
			false
		);
	}

	// 레벨에 맞는 기본 입력 모드로 복원
	RestoreDefaultInputMode();

	// BGM은 레벨에 배치된 DRBGMActor가 담당
}

void ADRPlayerController::HandleToggleSettings()
{
	ToggleSettingsMenu();
}

void ADRPlayerController::HandleSpectateNext()
{
	if (bIsSpectating)
	{
		SpectateNextPlayer();
	}
}

void ADRPlayerController::HandleSpectatePrevious()
{
	if (bIsSpectating)
	{
		SpectatePreviousPlayer();
	}
}

void ADRPlayerController::SetSpectateTarget(ACharacter* NewTarget)
{
	// 클라이언트: 서버에 요청 + 로컬에서 ViewTarget 설정
	if (!HasAuthority())
	{
		ServerSetSpectateTarget(NewTarget);

		// 클라이언트에서도 ViewTarget과 캐릭터 캐시 설정
		CurrentSpectatedCharacter = NewTarget;
		if (NewTarget)
		{
			SetViewTarget(NewTarget);
		}
		return;
	}

	// 서버: 전체 로직 실행

	// 이전 대상의 사망 델리게이트 해제
	if (CurrentSpectatedCharacter.IsValid())
	{
		if (ADRCharacterBase* OldTarget = Cast<ADRCharacterBase>(CurrentSpectatedCharacter.Get()))
		{
			OldTarget->OnDeathDelegate.RemoveDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
		}
	}

	CurrentSpectatedCharacter = NewTarget;

	if (NewTarget)
	{
		// ViewTarget 설정
		SetViewTarget(NewTarget);

		// UI 업데이트
		ClientUpdateSpectatorUI(NewTarget);

		// 새 대상의 사망 델리게이트 바인딩
		if (ADRCharacterBase* DRTarget = Cast<ADRCharacterBase>(NewTarget))
		{
			DRTarget->OnDeathDelegate.AddDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
		}
	}
}

void ADRPlayerController::ServerSetSpectateTarget_Implementation(ACharacter* NewTarget)
{
	// 서버에서 SetSpectateTarget 실행
	SetSpectateTarget(NewTarget);
}

void ADRPlayerController::ClientUpdateSpectatorUI_Implementation(ACharacter* SpectatedTarget)
{
	// 클라이언트에서만 실행
	if (!IsLocalController()) return;

	UpdateSpectatorUI(SpectatedTarget);
}

void ADRPlayerController::UpdateSpectatorUI(ACharacter* SpectatedTarget)
{
	if (!SpectatedTarget) return;

	// HUD 가져오기
	ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD());
	if (!DRHUD) return;

	// 관전 대상의 PlayerState 가져오기
	ADRPlayerState* SpectatedPS = SpectatedTarget->GetPlayerState<ADRPlayerState>();
	if (!SpectatedPS) return;

	// 관전 대상의 GAS 컴포넌트들 가져오기
	UAbilitySystemComponent* SpectatedASC = SpectatedPS->GetAbilitySystemComponent();
	UAttributeSet* SpectatedAS = SpectatedPS->GetAttributeSet();

	if (!SpectatedASC || !SpectatedAS) return;

	// 기존 WidgetController를 파괴하고 새로 만들어줌
	DRHUD->UpdateOverlayForSpectating(this, SpectatedPS, SpectatedASC, SpectatedAS);
}

void ADRPlayerController::OnSpectatedPlayerDied(AActor* DeadActor)
{
	if (!bIsSpectating) return;

	// 약간의 딜레이 후 다음 플레이어로 전환
	FTimerHandle SwitchTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(
		SwitchTimerHandle,
		[this]()
		{
			if (IsValid(this) && bIsSpectating)
			{
				SpectateNextPlayer();
			}
		},
		1.0f,
		false
	);
}

void ADRPlayerController::Move(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating) return;
	
	// �Է� ��� ���� Ȯ��
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed)) return;

	// 2D �Է��� ���� ��ǥ��� ��ȯ
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// ���� �̵� �Է� ����
	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);
	}
}

void ADRPlayerController::Look(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating) return;
	
	// ���콺 �ü� ó��
	const FVector2D Axis = InputActionValue.Get<FVector2D>();

	// Manager에서 감도 가져오기
	float Sensitivity = 1.0f;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDRSettingsManager* Manager = GI->GetSubsystem<UDRSettingsManager>())
		{
			if (UDRGameUserSettings* Settings = Manager->GetSettings())
			{
				Sensitivity = Settings->MouseSensitivity;
			}
		}
	}

	// 감도 적용
	AddYawInput(Axis.X * Sensitivity);
	AddPitchInput(Axis.Y * Sensitivity);
}

void ADRPlayerController::StartJump(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating) return;
	
	// ���� ����
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn<APawn>()))
	{
		ControlledCharacter->Jump();
	}
}

void ADRPlayerController::StopJump(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating) return;
	
	// ���� ����
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn<APawn>()))
	{
		ControlledCharacter->StopJumping();
	}
}

void ADRPlayerController::HandleInteract()
{
	if (bIsSpectating) return;
	
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;
	
	// 부품을 들고 있지 않을 때만 부품 획득 시도
	if (!DRCharacter->IsCarryingPart() && CurrentDetectedPart)
	{
		ServerRequestPickupPart(CurrentDetectedPart);
		return;
	}
	
	// 부품을 들고 있고 클렌저 사이트 오버랩 중이면 설치
	if (DRCharacter->IsCarryingPart())
	{
		// 클렌저 사이트 범위 안이면 설치
		if (CurrentOverlappedSite)
		{
			ServerRequestInstallPartToSite(CurrentOverlappedSite);
			return;
		}
		// 클렌저 사이트 범위 밖이면 떨어트리기
		else
		{
			ServerRequestDropPart();
			return;
		}
	}
	
	OnInteractPressed.Broadcast();
}

void ADRPlayerController::ToggleSettingsMenu()
{
	if (bIsSettingsMenuOpen)
	{
		CloseSettingsMenu();
	}
	else
	{
		OpenSettingsMenu();
	}
}

void ADRPlayerController::OpenSettingsMenu()
{
	// 로컬 컨트롤러에서만 실행
	if (!IsLocalController()) return;

	// 이미 열려있으면 무시
	if (bIsSettingsMenuOpen) return;

	bIsSettingsMenuOpen = true;

	// 입력 모드 변경
	SetInputMode(FInputModeUIOnly());
	SetShowMouseCursor(true);

	// Blueprint에서 위젯 생성
	OnSettingsMenuOpened();
}

void ADRPlayerController::CloseSettingsMenu()
{
	// 로컬 컨트롤러에서만 실행
	if (!IsLocalController()) return;

	// 이미 닫혀있으면 무시
	if (!bIsSettingsMenuOpen) return;

	// Blueprint에서 위젯 제거
	OnSettingsMenuClosed();

	bIsSettingsMenuOpen = false;
	RestoreDefaultInputMode();
}

void ADRPlayerController::ClientCloseSettingsMenu_Implementation()
{
	CloseSettingsMenu();
}

void ADRPlayerController::RestoreDefaultInputMode()
{
	if (!IsLocalController()) return;

	if (IsInMainMenu())
	{
		// 메인메뉴: UI 모드
		SetInputMode(FInputModeUIOnly());
		SetShowMouseCursor(true);
	}
	else if (IsInTutorial())
	{
		// 튜토리얼: 게임 모드
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
	else if (IsInLobby())
	{
		// 로비 상태에 따라 분기
		ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
		if (LGS && (LGS->GetLobbyState() == ELobbyState::WaitingRoom
				  || LGS->GetLobbyState() == ELobbyState::Transitioning))
		{
			SetInputMode(FInputModeUIOnly());
			SetShowMouseCursor(true);
		}
		else
		{
			SetInputMode(FInputModeGameOnly());
			SetShowMouseCursor(false);
		}
	}
	else
	{
		// 스테이지: 게임 모드
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}

bool ADRPlayerController::IsInMainMenu() const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	FString CurrentLevelName = World->GetMapName();
	CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

	return CurrentLevelName.Contains(TEXT("MainMenu"));
}

bool ADRPlayerController::IsInLobby() const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	FString CurrentLevelName = World->GetMapName();
	CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

	return CurrentLevelName.Contains(TEXT("Lobby"));
}

bool ADRPlayerController::IsInGameLevel() const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	FString CurrentLevelName = World->GetMapName();
	CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

	return CurrentLevelName.Contains(TEXT("Stage1"));
}

bool ADRPlayerController::IsInTutorial() const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	FString CurrentLevelName = World->GetMapName();
	CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

	return CurrentLevelName.Contains(TEXT("Tutorial"));
}

void ADRPlayerController::Client_ShowGameOverUI_Implementation()
{
	// 이미 UI가 표시 중이면 무시
	if (CurrentResultWidget) return;

	// 위젯 클래스가 설정되지 않았으면 리턴
	if (!GameOverWidgetClass) return;

	// 게임 오버 위젯 생성
	CurrentResultWidget = CreateWidget<UUserWidget>(this, GameOverWidgetClass);
	if (CurrentResultWidget)
	{
		// 뷰포트에 추가
		CurrentResultWidget->AddToViewport(100);
	}
}

void ADRPlayerController::Client_ShowGameClearUI_Implementation()
{
	// 이미 UI가 표시 중이면 무시
	if (CurrentResultWidget) return;

	// 위젯 클래스가 설정되지 않았으면 리턴
	if (!GameClearWidgetClass) return;

	// 게임 클리어 위젯 생성
	CurrentResultWidget = CreateWidget<UUserWidget>(this, GameClearWidgetClass);
	if (CurrentResultWidget)
	{
		// 뷰포트에 추가
		CurrentResultWidget->AddToViewport(100);
	}
}

void ADRPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (bIsSpectating) return;
	
	// �Է� ��� Ȯ�� �� �����Ƽ �Է� ó��
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed)) return;

	if (GetASC())
	{
		GetASC()->AbilityInputTagPressed(InputTag);
	}
}

void ADRPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (bIsSpectating) return;
	
	// �Է� ���� ��� Ȯ��
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputReleased)) return;

	if (GetASC() == nullptr) return;
	GetASC()->AbilityInputTagReleased(InputTag);
}

void ADRPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	if (bIsSpectating) return;
		
	// �Է� Ȧ�� ��� Ȯ��
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputHeld)) return;

	if (GetASC() == nullptr) return;
	GetASC()->AbilityInputTagHeld(InputTag);
}

UDRAbilitySystemComponent* ADRPlayerController::GetASC()
{
	return Cast<UDRAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
}

void ADRPlayerController::ServerCheatSkipToNextPhase_Implementation()
{
	// GameMode 가져오기
	ADRStageGameMode* StageGameMode = GetWorld()->GetAuthGameMode<ADRStageGameMode>();
	if (!StageGameMode) return;

	// 페이즈 전환
	StageGameMode->TransitionToNextPhase();
}

void ADRPlayerController::ServerRequestPickupPart_Implementation(ADRCleanserPart* Part)
{
	if (!Part) return;

	// ĳ���� ��������
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// ��ǰ ȹ�� �õ�
	DRCharacter->PickupPart(Part);
}

void ADRPlayerController::ServerRequestDropPart_Implementation()
{
	// 캐릭터 가져오기
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// 부품 떨어트리기
	DRCharacter->DropCarriedPart();
}

void ADRPlayerController::RequestChangeClass(bool bNext)
{
	ServerRequestChangeClass(bNext);
}

void ADRPlayerController::ServerRequestChangeClass_Implementation(bool bNext)
{
	// 대기실 상태에서만 가능
	ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
	if (!LGS || LGS->GetLobbyState() != ELobbyState::WaitingRoom) return;

	ADRPlayerState* PS = GetPlayerState<ADRPlayerState>();
	if (!PS) return;

	// 현재 클래스 가져오기
	int32 CurrentIndex = static_cast<int32>(PS->GetSelectedPlayerClass());
	constexpr int32 ClassCount = 2; // GardenRobot, VendingMachineRobot

	// 순환
	int32 NewIndex;
	if (bNext)
		NewIndex = (CurrentIndex + 1) % ClassCount;
	else
		NewIndex = (CurrentIndex - 1 + ClassCount) % ClassCount;

	PS->SetSelectedPlayerClass(static_cast<EPlayerCharacterClass>(NewIndex));
}

void ADRPlayerController::ClientTeleportToSlot_Implementation(FVector SlotLocation, FRotator SlotRotation)
{
	if (APawn* MyPawn = GetPawn())
	{
		MyPawn->TeleportTo(SlotLocation, SlotRotation);

		if (UCharacterMovementComponent* MovementComp =
			Cast<UCharacterMovementComponent>(MyPawn->GetMovementComponent()))
		{
			MovementComp->Velocity = FVector::ZeroVector;
			MovementComp->DisableMovement();
		}
	}
}

void ADRPlayerController::ClientSetWaitingRoomView_Implementation(
	ADRWaitingRoomCameraActor* CameraActor)
{
	if (!CameraActor) return;

	// 카메라 캐시 저장 (ClientRestart에서 재고정에 사용)
	CachedWaitingRoomCamera = CameraActor;

	bIsInWaitingRoom = true;
	bAutoManageActiveCameraTarget = false; // Pawn 변경 시 자동 ViewTarget 전환 차단

	// 잘못 생성된 LobbyOverlay가 있으면 제거
	if (ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD()))
	{
		DRHUD->RemoveOverlay();
	}

	// 즉시 고정 카메라로 전환
	SetViewTargetWithBlend(CameraActor, 0.f);

	// UI Only 모드 (마우스 커서 ON)
	SetInputMode(FInputModeUIOnly());
	SetShowMouseCursor(true);

	// 소유 Pawn의 메시 가시성 전환 (1P 숨기기, 3P 보이기)
	if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(GetPawn()))
	{
		DRCharacter->SetWaitingRoomVisibility(true);
	}

	// 대기실 UI 생성 (아직 없으면)
	CreateWaitingRoomUI();
}

void ADRPlayerController::ClientStartCameraTransitionToCharacter_Implementation()
{
	bIsInWaitingRoom = false;

	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		// Pawn이 아직 리플리케이트되지 않음 → 0.1초 간격으로 재시도
		int32 MaxRetries = 50; // 5초 제한
		GetWorldTimerManager().SetTimer(
			CameraTransitionRetryHandle,
			[this, MaxRetries, RetryCount = 0]() mutable
			{
				if (!IsValid(this)) return;

				if (++RetryCount > MaxRetries)
				{
					UE_LOG(LogTemp, Warning, TEXT("CameraTransition: Pawn not replicated after %d retries, forcing input mode"), MaxRetries);
					GetWorldTimerManager().ClearTimer(CameraTransitionRetryHandle);
					// 최소한 InputMode 전환으로 완전 고착 방지
					bAutoManageActiveCameraTarget = true;
					SetInputMode(FInputModeGameOnly());
					SetShowMouseCursor(false);
					InitOverlayForFreeRoam();
					return;
				}

				APawn* Pawn = GetPawn();
				if (!Pawn) return; // 아직 없으면 다음 반복에서 재시도

				// Pawn 도착 완료 → 타이머 중지 + 카메라 전환 실행
				GetWorldTimerManager().ClearTimer(CameraTransitionRetryHandle);
				ExecuteCameraTransitionToCharacter();
			},
			0.1f,
			true
		);
		return;
	}

	// Pawn이 이미 있으면 즉시 실행 (호스트 또는 빠른 리플리케이션)
	ExecuteCameraTransitionToCharacter();
}

void ADRPlayerController::ExecuteCameraTransitionToCharacter()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn) return;

	// 메시 가시성 복원 (3P 숨기기, 1P 보이기 — 일반 FPS 모드)
	if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(MyPawn))
	{
		DRCharacter->SetWaitingRoomVisibility(false);
	}

	// 고정 카메라 → 캐릭터 카메라로 1.5초간 부드럽게 블렌드
	SetViewTargetWithBlend(MyPawn, 1.5f, EViewTargetBlendFunction::VTBlend_EaseInOut);

	// 전환 완료 후 인풋 모드 변경 + HUD 오버레이 초기화
	FTimerHandle InputTimerHandle;
	GetWorldTimerManager().SetTimer(
		InputTimerHandle,
		[this]()
		{
			if (!IsValid(this)) return;

			// (1) 블렌드 완료 후에야 자동 카메라 관리 활성화
			bAutoManageActiveCameraTarget = true;

			// (2) ViewTarget을 Pawn으로 확정 (블렌드 잔여 상태 정리)
			if (APawn* FinalPawn = GetPawn())
			{
				SetViewTarget(FinalPawn);

				// (3) ControlRotation을 Pawn의 현재 회전으로 동기화
				SetControlRotation(FinalPawn->GetActorRotation());
			}

			// (4) 입력 모드 전환
			SetInputMode(FInputModeGameOnly());
			SetShowMouseCursor(false);

			// (5) HUD 오버레이 초기화 (대기실에서 스킵했으므로)
			InitOverlayForFreeRoam();
		},
		1.5f,
		false
	);
}

void ADRPlayerController::OnRep_Pawn()
{
	// 엔진 기본 처리 (AcknowledgePossession 등)
	Super::OnRep_Pawn();

	// 대기실에서는 엔진이 변경한 ViewTarget을 즉시 복원
	if (bIsInWaitingRoom && CachedWaitingRoomCamera.IsValid())
	{
		bAutoManageActiveCameraTarget = false;
		SetViewTargetWithBlend(CachedWaitingRoomCamera.Get(), 0.f);
	}
}

void ADRPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
	// 대기실에서는 ViewTarget 자동 전환만 차단하고, 나머지 엔진 초기화는 정상 수행
	if (bIsInWaitingRoom)
	{
		// bAutoManageActiveCameraTarget = false로 Super 내부의 ViewTarget 자동 전환 차단
		bAutoManageActiveCameraTarget = false;

		// Super 호출: ResetIgnoreInputFlags, AcknowledgePossession, PawnClientRestart 등
		// 엔진 초기화를 정상 수행 (호스트에서 Look/Move 입력이 무시되는 버그 방지)
		Super::ClientRestart_Implementation(NewPawn);

		// 새 폰에 대기실 가시성 적용
		if (NewPawn)
		{
			if (ADRCharacter* DRChar = Cast<ADRCharacter>(NewPawn))
			{
				DRChar->SetWaitingRoomVisibility(true);
			}
		}

		// Super가 ViewTarget을 변경했을 수 있으므로 캐시된 카메라로 즉시 복원
		if (CachedWaitingRoomCamera.IsValid())
		{
			SetViewTargetWithBlend(CachedWaitingRoomCamera.Get(), 0.f);
		}

		return;
	}

	Super::ClientRestart_Implementation(NewPawn);
}

void ADRPlayerController::ClientKicked_Implementation(const FString& Reason)
{
	// 대기실 UI 제거
	DestroyWaitingRoomUI();

	// 세션 떠나기 → 메인 메뉴로 이동
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMultiplayerSessionsSubsystem* Subsystem = GI->GetSubsystem<UMultiplayerSessionsSubsystem>())
		{
			Subsystem->LeaveServer();
		}
	}
}

// ========== 대기실 UI ==========

void ADRPlayerController::CreateWaitingRoomUI()
{
	if (!IsLocalController()) return;
	if (WaitingRoomWidget) return; // 이미 존재
	if (!WaitingRoomWidgetClass) return;

	WaitingRoomWidget = CreateWidget<UDRWaitingRoomWidget>(this, WaitingRoomWidgetClass);
	if (WaitingRoomWidget)
	{
		WaitingRoomWidget->AddToViewport();

		// 호스트 여부 설정 (Listen Server의 로컬 컨트롤러 = 호스트)
		bool bIsHost = HasAuthority();
		WaitingRoomWidget->SetIsHost(bIsHost);

		// LobbyState 변경 구독
		if (ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>())
		{
			LGS->OnLobbyStateChanged.AddDynamic(this, &ADRPlayerController::OnLobbyStateChangedForUI);
		}

		RefreshWaitingRoomUI();
	}
}

void ADRPlayerController::DestroyWaitingRoomUI()
{
	// 델리게이트 해제
	if (UWorld* World = GetWorld())
	{
		if (ADRLobbyGameState* LGS = World->GetGameState<ADRLobbyGameState>())
		{
			LGS->OnLobbyStateChanged.RemoveDynamic(this, &ADRPlayerController::OnLobbyStateChangedForUI);
		}
	}

	if (WaitingRoomWidget)
	{
		WaitingRoomWidget->RemoveFromParent();
		WaitingRoomWidget = nullptr;
	}
}

void ADRPlayerController::ClientRefreshWaitingRoomUI_Implementation()
{
	RefreshWaitingRoomUI();
}

void ADRPlayerController::RefreshWaitingRoomUI()
{
	if (!WaitingRoomWidget) return;

	ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
	ADRGameStateBase* GS = GetWorld()->GetGameState<ADRGameStateBase>();
	if (!LGS || !GS) return;

	// 로컬 PlayerState 유효성 검사 + 재시도
	APlayerState* MyPS = GetPlayerState<APlayerState>();
	if (!MyPS)
	{
		FTimerHandle RetryTimer;
		GetWorldTimerManager().SetTimer(
			RetryTimer,
			[WeakThis = TWeakObjectPtr<ADRPlayerController>(this)]()
			{
				if (ADRPlayerController* PC = WeakThis.Get())
				{
					PC->RefreshWaitingRoomUI();
				}
			},
			0.2f, false
		);
		return;
	}

	TArray<FWaitingRoomPlayerInfo> Infos;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) continue;
		ADRPlayerState* DRPS = Cast<ADRPlayerState>(PS);
		if (!DRPS) continue;

		FWaitingRoomPlayerInfo Info;
		Info.PlayerName = PS->GetPlayerName();
		Info.SelectedClass = DRPS->GetSelectedPlayerClass();
		Info.bIsHost = GS->IsPlayerHost(PS);
		Info.OwningPlayerState = PS;

		// 서버 권위 슬롯 인덱스 사용
		Info.SlotIndex = DRPS->GetWaitingRoomSlotIndex();

		// bIsLocalPlayer 판정 (PlayerId 비교)
		Info.bIsLocalPlayer = (PS->GetPlayerId() == MyPS->GetPlayerId());

		Infos.Add(Info);
	}

	// SlotIndex 기준으로 정렬하여 UI에 전달
	Infos.Sort([](const FWaitingRoomPlayerInfo& A, const FWaitingRoomPlayerInfo& B)
	{
		return A.SlotIndex < B.SlotIndex;
	});

	WaitingRoomWidget->RefreshPlayerSlots(Infos);
}

void ADRPlayerController::OnLobbyStateChangedForUI(ELobbyState NewState)
{
	if (NewState == ELobbyState::Transitioning || NewState == ELobbyState::FreeRoam)
	{
		DestroyWaitingRoomUI();
	}
}

void ADRPlayerController::InitOverlayForFreeRoam()
{
	ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD());
	if (!DRHUD) return;

	ADRPlayerState* PS = GetPlayerState<ADRPlayerState>();
	if (!PS) return;

	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	UAttributeSet* AS = PS->GetAttributeSet();
	if (!ASC || !AS) return;

	DRHUD->InitOverlay(this, PS, ASC, AS);

	// 위젯 트리 완전 초기화 후 어빌리티 아이콘 강제 갱신
	FTimerHandle AbilityIconTimerHandle;
	GetWorldTimerManager().SetTimer(
		AbilityIconTimerHandle,
		[WeakThis = TWeakObjectPtr<ADRPlayerController>(this)]()
		{
			ADRPlayerController* PC = WeakThis.Get();
			if (!PC) return;

			ADRHUD* HUD = Cast<ADRHUD>(PC->GetHUD());
			if (!HUD) return;

			ADRPlayerState* PlayerState = PC->GetPlayerState<ADRPlayerState>();
			if (!PlayerState) return;

			UAbilitySystemComponent* AbilitySystem = PlayerState->GetAbilitySystemComponent();
			UAttributeSet* Attributes = PlayerState->GetAttributeSet();
			if (!AbilitySystem || !Attributes) return;

			const FWidgetControllerParams Params(PC, PlayerState, AbilitySystem, Attributes);
			if (UOverlayWidgetController* WC = HUD->GetOverlayWidgetController(Params))
			{
				WC->BroadcastAbilityInfo();
			}
		},
		0.1f,
		false
	);
}

void ADRPlayerController::ServerRequestPowerOn_Implementation()
{
	// 호스트 검증: Server RPC이므로 서버에서 실행됨
	// IsLocalController()로 호스트(Listen Server) 확인
	if (!IsLocalController() || !HasAuthority()) return;

	ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>();
	if (LobbyGM)
	{
		LobbyGM->PowerOn(this);
	}
}

void ADRPlayerController::ServerRequestKickPlayer_Implementation(APlayerState* TargetPlayerState)
{
	// 호스트 검증
	if (!IsLocalController() || !HasAuthority()) return;
	if (!TargetPlayerState) return;

	// TargetPlayerState → PlayerController 찾기
	APlayerController* TargetPC = Cast<APlayerController>(TargetPlayerState->GetOwner());
	ADRPlayerController* TargetDRPC = Cast<ADRPlayerController>(TargetPC);

	ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>();
	if (LobbyGM && TargetDRPC)
	{
		LobbyGM->KickPlayer(this, TargetDRPC);
	}
}
