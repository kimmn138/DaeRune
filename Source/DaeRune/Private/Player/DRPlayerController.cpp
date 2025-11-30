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
	if (!HasAuthority() || !Site) return;
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
	// �����Ƽ �Է� ���ε� (InputConfig ���)
	DRInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
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
	AddYawInput(Axis.X);
	AddPitchInput(Axis.Y);
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
	if (DRCharacter->IsCarryingPart() && CurrentOverlappedSite)
	{
		ServerRequestInstallPartToSite(CurrentOverlappedSite);
		return;
	}
	
	OnInteractPressed.Broadcast();
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
	// 서버에서만 실행되는 RPC
	if (!HasAuthority()) return;

	// GameMode 가져오기
	ADRStageGameMode* StageGameMode = GetWorld()->GetAuthGameMode<ADRStageGameMode>();
	if (!StageGameMode) return;

	// 페이즈 전환
	StageGameMode->TransitionToNextPhase();
}

void ADRPlayerController::ServerRequestPickupPart_Implementation(ADRCleanserPart* Part)
{
	if (!HasAuthority() || !Part) return;

	// ĳ���� ��������
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// ��ǰ ȹ�� �õ�
	DRCharacter->PickupPart(Part);
}
