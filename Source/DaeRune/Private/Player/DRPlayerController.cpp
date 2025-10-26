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
#include "Character/DRCharacter.h"
#include "Camera/CameraComponent.h"

ADRPlayerController::ADRPlayerController()
{
	// 멀티플레이 리플리케이션 활성화
	bReplicates = true;

	// 부품 시스템 초기화
	bPartDetectionEnabled = false;
	CurrentDetectedPart = nullptr;
	NearbyPart = nullptr;
	LineTraceTimer = 0.f;
}

void ADRPlayerController::OnCorruptedStateChanged(bool bIsStateChanged)
{
	// 부패 상태 플래그 업데이트
	bIsCorrupted = bIsStateChanged;

	// 음성 채팅 설정
	//SetVoiceChatEnabled(!bIsCorrupted);

	// 팀 구분 시각 효과 설정
	//SetTeamVisualsEnabled(!bIsCorrupted);
}

void ADRPlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter)
{
	// 로컬 컨트롤러에서만 데미지 텍스트 표시
	if (IsValid(TargetCharacter) && DamageTextComponentClass && IsLocalController())
	{
		// 데미지 텍스트 컴포넌트 생성 및 설정
		UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);
		DamageText->RegisterComponent();

		// 타겟 캐릭터에 일시적으로 부착 후 분리 (월드 위치 유지)
		DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

		// 데미지 수치 설정 및 애니메이션 시작
		DamageText->SetDamageText(DamageAmount);
	}
}

void ADRPlayerController::SetPartDetectionEnabled(bool bEnabled, ADRCleanserPart* Part)
{
	if (bEnabled)
	{
		// 라인트레이싱 활성화
		bPartDetectionEnabled = true;
		NearbyPart = Part;
		LineTraceTimer = 0.f;
	}
	else
	{
		// 해당 부품이 현재 근처 부품과 같을 때만 비활성화
		if (NearbyPart == Part)
		{
			bPartDetectionEnabled = false;
			NearbyPart = nullptr;
			
			// 현재 감지된 부품이 있으면 UI 숨김 알림
			if (CurrentDetectedPart)
			{
				CurrentDetectedPart->OnLineTraceLost(this);
				CurrentDetectedPart = nullptr;
			}
		}
	}
}

ADRCleanserPart* ADRPlayerController::FindPartByLineTrace()
{
	// 캐릭터 가져오기
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return nullptr;

	// 캐릭터가 이미 부품을 들고 있으면 감지하지 않음
	if (DRCharacter->IsCarryingPart()) return nullptr;

	// 카메라 컴포넌트 가져오기
	UCameraComponent* Camera = DRCharacter->FindComponentByClass<UCameraComponent>();
	if (!Camera) return nullptr;

	// 라인트레이싱 시작/끝 위치 계산
	FVector Start = Camera->GetComponentLocation();
	FVector End = Start + Camera->GetForwardVector() * LineTraceDistance;

	// 라인트레이싱 실행
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

	// 부품에 히트했는지 확인
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

void ADRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Enhanced Input Context가 설정되어 있는지 확인
	check(DRContext);

	// Enhanced Input 서브시스템에 매핑 컨텍스트 추가
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->AddMappingContext(DRContext, 0);
	}

	// UI 설정
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());

	// 플레이어는 TeamId 0
	SetGenericTeamId(FGenericTeamId(0));
}

void ADRPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// 부품 감지가 비활성화되어 있으면 스킵
	if (!bPartDetectionEnabled) return;

	// 라인트레이싱 타이머 업데이트
	LineTraceTimer += DeltaTime;
	if (LineTraceTimer >= LineTraceUpdateInterval)
	{
		LineTraceTimer = 0.f;

		// 부품 감지
		ADRCleanserPart* DetectedPart = FindPartByLineTrace();

		// 감지된 부품이 변경되었는지 확인
		if (DetectedPart != CurrentDetectedPart)
		{
			// 이전에 감지된 부품이 있으면 알림
			if (CurrentDetectedPart)
			{
				CurrentDetectedPart->OnLineTraceLost(this);
			}

			CurrentDetectedPart = DetectedPart;

			// 새로 감지된 부품이 있으면 알림
			if (CurrentDetectedPart)
			{
				CurrentDetectedPart->OnLineTraceDetected(this);
			}
		}
	}
}

void ADRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// DRInputComponent로 캐스팅
	UDRInputComponent* DRInputComponent = CastChecked<UDRInputComponent>(InputComponent);
	// 기본 입력 액션 바인딩
	DRInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Move);
	DRInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Look);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ADRPlayerController::StartJump);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ADRPlayerController::StopJump);
	DRInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &ADRPlayerController::HandleInteract);
	// 어빌리티 입력 바인딩 (InputConfig 기반)
	DRInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}

void ADRPlayerController::Move(const FInputActionValue& InputActionValue)
{
	// 입력 블록 상태 확인
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed)) return;

	// 2D 입력을 월드 좌표계로 변환
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// 폰에 이동 입력 전달
	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);
	}
}

void ADRPlayerController::Look(const FInputActionValue& InputActionValue)
{
	// 마우스 시선 처리
	const FVector2D Axis = InputActionValue.Get<FVector2D>();
	AddYawInput(Axis.X);
	AddPitchInput(Axis.Y);
}

void ADRPlayerController::StartJump(const FInputActionValue& InputActionValue)
{
	// 점프 시작
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn<APawn>()))
	{
		ControlledCharacter->Jump();
	}
}

void ADRPlayerController::StopJump(const FInputActionValue& InputActionValue)
{
	// 점프 종료
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn<APawn>()))
	{
		ControlledCharacter->StopJumping();
	}
}

void ADRPlayerController::HandleInteract()
{
	// 부품 획득 시도
	if (CurrentDetectedPart)
	{
		ServerRequestPickupPart(CurrentDetectedPart);
		return;
	}

	// 기존 델리게이트 브로드캐스트 (클렌저 사이트 설치용)
	OnInteractPressed.Broadcast();
}

void ADRPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	// 입력 블록 확인 후 어빌리티 입력 처리
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed)) return;

	if (GetASC())
	{
		GetASC()->AbilityInputTagPressed(InputTag);
	}
}

void ADRPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	// 입력 해제 블록 확인
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputReleased)) return;

	if (GetASC() == nullptr) return;
	GetASC()->AbilityInputTagReleased(InputTag);
}

void ADRPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	// 입력 홀드 블록 확인
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputHeld)) return;

	if (GetASC() == nullptr) return;
	GetASC()->AbilityInputTagHeld(InputTag);
}

UDRAbilitySystemComponent* ADRPlayerController::GetASC()
{
	// ASC 캐싱을 통한 성능 최적화
	if (DRAbilitySystemComponent == nullptr)
	{
		DRAbilitySystemComponent = Cast<UDRAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
	}
	return DRAbilitySystemComponent;
}

void ADRPlayerController::ServerRequestPickupPart_Implementation(ADRCleanserPart* Part)
{
	if (!HasAuthority() || !Part) return;

	// 캐릭터 가져오기
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// 부품 획득 시도
	DRCharacter->PickupPart(Part);
}
