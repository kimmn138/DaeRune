// Copyright DaeRune


#include "Player/DRPlayerController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DRGameplayTags.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "Input/DRInputComponent.h"
#include "GameFramework/Character.h"
#include "UI/Widget/DamageTextComponent.h"

ADRPlayerController::ADRPlayerController()
{
	bReplicates = true; // 복제 활성화 설정임
}

void ADRPlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter) // RPC 구현부임
{
	// 유효성 및 로컬 컨트롤러 검사임
	if (IsValid(TargetCharacter) && DamageTextComponentClass && IsLocalController())
	{
		// 데미지 텍스트 컴포넌트 생성 및 설정임
		UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);
		DamageText->RegisterComponent();
		DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		DamageText->SetDamageText(DamageAmount); // 텍스트 내용 설정임
	}
}

void ADRPlayerController::BeginPlay() // 시작 로직 구현부임
{
	Super::BeginPlay();
	check(DRContext); // 매핑 컨텍스트 유효성 검사임

	// Enhanced Input 서브시스템에 컨텍스트 추가임
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->AddMappingContext(DRContext, 0);
	}

	// 커서 숨김 및 게임 전용 입력 모드 설정임
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

void ADRPlayerController::SetupInputComponent() // 입력 바인딩 구현부임
{
	Super::SetupInputComponent();

	// DR 전용 입력 컴포넌트 캐스팅 및 바인딩임
	UDRInputComponent* DRInputComponent = CastChecked<UDRInputComponent>(InputComponent);
	DRInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Move);
	DRInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Look);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ADRPlayerController::Jump);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ADRPlayerController::StopJump);
	DRInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}

void ADRPlayerController::Move(const FInputActionValue& InputActionValue) // 이동 입력 처리 구현부임
{
	// 입력 차단 태그 검사임
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed))
	{
		return; 
	}
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>(); // 축 값 획득임
	const FRotator Rotation = GetControlRotation(); // 뷰 회전값 획득임
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f); // 수평 회전만 적용임

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X); // 전방 벡터 임
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y); // 우측 벡터 임

	if (APawn* ControlledPawn = GetPawn<APawn>()) // 제어 Pawn 유효성 검사임
	{
		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y); // 전/후 이동 입력임
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X); // 좌/우 이동 입력임
	}
}

void ADRPlayerController::Look(const FInputActionValue& InputActionValue) // 시점 입력 처리 구현부임
{
	const FVector2D Axis = InputActionValue.Get<FVector2D>(); // 축 값 획득임
	AddYawInput(Axis.X); // 좌우 시점 입력임
	AddPitchInput(Axis.Y); // 상하 시점 입력임
}

void ADRPlayerController::Jump(const FInputActionValue& InputActionValue) // 점프 입력 처리 구현부임
{
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn<APawn>())) // 캐릭터 유효성 검사
	{
		ControlledCharacter->Jump(); // 점프
	}
}

void ADRPlayerController::StopJump(const FInputActionValue& InputActionValue) // 점프 중단 입력 처리 구현부임
{
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn<APawn>())) // 캐릭터 유효성 검사
	{
		ControlledCharacter->StopJumping(); // 점프 중단
	}
}

void ADRPlayerController::AbilityInputTagPressed(FGameplayTag InputTag) // 능력 입력 시작 처리 구현부임
{
	// 입력 차단 태그 검사임
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed))
	{
		return;
	}
	if (GetASC()) GetASC()->AbilityInputTagPressed(InputTag); // ASC에 위임임
}

void ADRPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)  // 능력 입력 해제 처리 구현부임
{
	// 입력 차단 태그 검사임
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputReleased))
	{
		return;
	}
	if (GetASC() == nullptr) return; // ASC 유효성 검사임
	GetASC()->AbilityInputTagReleased(InputTag); // ASC에 위임임
}

void ADRPlayerController::AbilityInputTagHeld(FGameplayTag InputTag) // 능력 입력 유지 처리 구현부임
{
	// 입력 차단 태그 검사임
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputHeld))
	{
		return;
	}
	if (GetASC() == nullptr) return; // ASC 유효성 검사임
	GetASC()->AbilityInputTagHeld(InputTag); // ASC에 위임임
}

UDRAbilitySystemComponent* ADRPlayerController::GetASC() // ASC 반환 헬퍼 구현부임
{
	if (DRAbilitySystemComponent == nullptr)
	{
		// Pawn의 ASC 획득 및 캐스팅임
		DRAbilitySystemComponent = Cast<UDRAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
	}
	return DRAbilitySystemComponent; // ASC 반환임
}
