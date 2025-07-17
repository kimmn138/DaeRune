// Copyright DaeRune


#include "Character/DRCharacter.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "NiagaraComponent.h"
#include "AbilitySystem/Debuff/DebuffNiagaraComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UI/HUD/DRHUD.h"

ADRCharacter::ADRCharacter()
{
	// 무브먼트 방향 회전 설정
	GetCharacterMovement()->bOrientRotationToMovement = true;
	// 회전 속도 설정
	GetCharacterMovement()->RotationRate = FRotator(0.f, 400.f, 0.f);
	// 평면 이동 제한 설정
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// 스프링 암 컴포넌트 생성 및 부모 캡슐 연결
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->SetRelativeLocation(FVector(30.f, 0.f, 50.f));
	CameraBoom->TargetArmLength = 0.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

	// 팔로우 카메라 컴포넌트 생성 및 붐 컴포넌트 연결
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 컨트롤러 회전 사용 설정
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = true;
}

// 서버 소유 시 어빌리티 초기화 및 부여 처리
void ADRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 어빌리티 액터 정보 초기화
	InitAbilityActorInfo();
	// 캐릭터 어빌리티 부여
	AddCharacterAbilities();
}

// 플레이어 상태 복제 응답 시 어빌리티 정보 동기화 처리
void ADRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트 어빌리티 액터 정보 초기화
	InitAbilityActorInfo();
}

// 플레이어 레벨 반환 구현
int32 ADRCharacter::GetPlayerLevel_Implementation()
{
	const ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();
	check(DRPlayerState);
	return DRPlayerState->GetPlayerLevel();
}

// 플레이어 클래스 반환 구현
EPlayerCharacterClass ADRCharacter::GetPlayerCharacterClass_Implementation()
{
	return CharacterClass;
}

// 기절 상태 복제 응답 처리
void ADRCharacter::OnRep_Stunned()
{
	if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(AbilitySystemComponent))
	{
		// 차단 태그 컨테이너 생성
		const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
		FGameplayTagContainer BlockedTags;
		BlockedTags.AddTag(GameplayTags.Player_Block_InputHeld);
		BlockedTags.AddTag(GameplayTags.Player_Block_InputPressed);
		BlockedTags.AddTag(GameplayTags.Player_Block_InputReleased);
		if (bIsStunned)
		{
			// 기절 태그 추가 및 디버프 활성화
			DRASC->AddLooseGameplayTags(BlockedTags);
			StunDebuffComponent->Activate();
		}
		else
		{
			// 기절 태그 제거 및 디버프 비활성화
			DRASC->RemoveLooseGameplayTags(BlockedTags);
			StunDebuffComponent->Deactivate();
		}
	}
}

// 화상 상태 복제 응답 처리
void ADRCharacter::OnRep_Burned()
{
	if (bIsBurned)
	{
		// 화상 디버프 활성화
		BurnDebuffComponent->Activate();
	}
	else
	{
		// 화상 디버프 비활성화
		BurnDebuffComponent->Deactivate();
	}
}

// 기본 특성 초기화 호출
void ADRCharacter::InitializeDefaultAttributes() const
{
	UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes(this, CharacterClass, 1.0f, AbilitySystemComponent);
}

// 어빌리티 액터 정보 초기화 및 HUD 등록 처리
void ADRCharacter::InitAbilityActorInfo()
{
	ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();
	check(DRPlayerState);
	// AbilitySystemComponent 초기화
	DRPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(DRPlayerState, this);
	// 액터 정보 설정 콜백 호출
	Cast<UDRAbilitySystemComponent>(DRPlayerState->GetAbilitySystemComponent())->AbilityActorInfoSet();
	AbilitySystemComponent = DRPlayerState->GetAbilitySystemComponent();
	AttributeSet = DRPlayerState->GetAttributeSet();
	// ASC 등록 이벤트 브로드캐스트
	OnAscRegistered.Broadcast(AbilitySystemComponent);
	// 스턴 태그 이벤트 리스너 등록
	AbilitySystemComponent->RegisterGameplayTagEvent(FDRGameplayTags::Get().Debuff_Stun, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ADRCharacter::StunTagChanged);

	if (ADRPlayerController* DRPlayerController = Cast<ADRPlayerController>(GetController()))
	{
		if (ADRHUD* DRHUD = Cast<ADRHUD>(DRPlayerController->GetHUD()))
		{
			// HUD 오버레이 초기화
			DRHUD->InitOverlay(DRPlayerController, DRPlayerState, AbilitySystemComponent, AttributeSet);
		}
	}
	// 어트리뷰트 기본값 설정
	InitializeDefaultAttributes();
}
