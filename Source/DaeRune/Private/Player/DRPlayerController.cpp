// Copyright DaeRune


#include "Player/DRPlayerController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DRGameplayTags.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "Input/DRInputComponent.h"
#include "GameFramework/Character.h"
#include "UI/Widget/DamageTextComponent.h"
#include "Actor/DRCleanserPart.h"
#include "Actor/DRCleanserSite.h"
#include "Character/DRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "UI/HUD/DRHUD.h"
#include "Player/DRPlayerState.h"
#include "UI/WidgetController/DRWidgetController.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "UI/Widget/DRSettingsWidget.h"
#include "Game/DRSettingsManager.h"
#include "Game/DRGameUserSettings.h"

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
    
	// 자기 자신으로 ViewTarget 복원
	if (GetPawn())
	{
		SetViewTarget(GetPawn());
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

		// 현재 레벨이 메인메뉴인지 체크
		UWorld* World = GetWorld();
		if (World)
		{
			FString CurrentLevelName = World->GetMapName();
			CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

			// 메인메뉴면 UI 입력 모드로 설정
			if (CurrentLevelName.Contains(TEXT("MainMenu")))
			{
				SetInputMode(FInputModeUIOnly());
				SetShowMouseCursor(true);
			}
		}
	}
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
	// 서버에 요청
	if (!HasAuthority())
	{
		ServerSetSpectateTarget(NewTarget);
		return;
	}

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

	// 위젯이 없으면 생성
	if (!SettingsWidget && SettingsWidgetClass)
	{
		SettingsWidget = CreateWidget<UDRSettingsWidget>(this, SettingsWidgetClass);
		if (SettingsWidget)
		{
			SettingsWidget->AddToViewport(100); // 높은 Z-Order로 다른 UI 위에 표시
			SettingsWidget->SetVisibility(ESlateVisibility::Collapsed); // 처음엔 숨김
		}
	}

	if (SettingsWidget)
	{
		SettingsWidget->OpenSettings();
		bIsSettingsMenuOpen = true;

		// 입력 모드 변경
		SetInputMode(FInputModeUIOnly());
		SetShowMouseCursor(true);
	}
}

void ADRPlayerController::CloseSettingsMenu()
{
	// 로컬 컨트롤러에서만 실행
	if (!IsLocalController()) return;

	// 이미 닫혀있으면 무시
	if (!bIsSettingsMenuOpen) return;

	if (SettingsWidget)
	{
		SettingsWidget->CloseSettings();
		bIsSettingsMenuOpen = false;

		// 현재 레벨이 메인메뉴인지 체크
		UWorld* World = GetWorld();
		if (World)
		{
			FString CurrentLevelName = World->GetMapName();
			CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

			// 메인메뉴면 UI 모드 유지
			if (CurrentLevelName.Contains(TEXT("MainMenu")))
			{
				SetInputMode(FInputModeUIOnly());
				SetShowMouseCursor(true);
			}
			else
			{
				// 게임 레벨이면 게임 모드로
				SetInputMode(FInputModeGameOnly());
				SetShowMouseCursor(false);
			}
		}
	}
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

		// 입력 모드를 UI로 변경
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(CurrentResultWidget->TakeWidget());
		SetInputMode(InputMode);

		// 마우스 커서 표시
		bShowMouseCursor = true;
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

		// 입력 모드를 UI로 변경
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(CurrentResultWidget->TakeWidget());
		SetInputMode(InputMode);

		// 마우스 커서 표시
		bShowMouseCursor = true;
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
