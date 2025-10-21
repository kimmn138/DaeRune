// Copyright DaeRune


#include "Character/DRCharacter.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "NiagaraComponent.h"
#include "AbilitySystem/Debuff/DebuffNiagaraComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UI/HUD/DRHUD.h"
#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "Actor/DRCleanserPart.h"
#include "Net/UnrealNetwork.h"

ADRCharacter::ADRCharacter()
{
	// 이동 방향으로 회전 설정
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 400.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// 카메라 붐 설정
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->SetRelativeLocation(FVector(30.f, 0.f, 50.f));
	CameraBoom->TargetArmLength = 0.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

	// 따라다니는 카메라 설정
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 컨트롤러 회전 설정
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = true;

	// 기본 캐릭터 클래스는 엘리멘탈리스트
	CharacterClass = ECharacterClass::Elementalist;

	// 부품 시스템 초기화
	bIsCarryingPart = false;
	CarriedPart = nullptr;
}

void ADRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRCharacter, bIsCarryingPart);
	DOREPLIFETIME(ADRCharacter, CarriedPart);
}

void ADRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버에서 GAS 초기화 및 어빌리티 부여
	InitAbilityActorInfo();
	AddCharacterAbilities();
}

void ADRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트에서 GAS 초기화
	InitAbilityActorInfo();
}

void ADRCharacter::OnRep_Stunned()
{
	if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(AbilitySystemComponent))
	{
		const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
		// 입력 차단 태그들 설정
		FGameplayTagContainer BlockedTags;
		BlockedTags.AddTag(GameplayTags.Player_Block_InputHeld);
		BlockedTags.AddTag(GameplayTags.Player_Block_InputPressed);
		BlockedTags.AddTag(GameplayTags.Player_Block_InputReleased);
		if (bIsStunned)
		{
			// 스턴 시작: 입력 차단 + 스턴 이펙트 활성화
			DRASC->AddLooseGameplayTags(BlockedTags);
			StunDebuffComponent->Activate();
		}
		else
		{
			// 스턴 종료: 입력 복구 + 스턴 이펙트 비활성화
			DRASC->RemoveLooseGameplayTags(BlockedTags);
			StunDebuffComponent->Deactivate();
		}
	}
}

void ADRCharacter::OnRep_Burned()
{
	if (bIsBurned)
	{
		BurnDebuffComponent->Activate();
	}
	else
	{
		BurnDebuffComponent->Deactivate();
	}
}

bool ADRCharacter::PickupPart(ADRCleanserPart* Part)
{
	if (!HasAuthority() || !Part || bIsCarryingPart) return false;

	// 부품 획득 처리
	Part->PickupPart(this);

	// 상태 업데이트
	bIsCarryingPart = true;
	CarriedPart = Part;

	return true;
}

void ADRCharacter::InstallCarriedPart()
{
	if (!HasAuthority() || !bIsCarryingPart || !CarriedPart) return;

	// 부품 설치 처리
	CarriedPart->InstallPart();

	// 상태 초기화
	bIsCarryingPart = false;
	CarriedPart = nullptr;
}

void ADRCharacter::OnRep_bIsCarryingPart()
{
	// 클라이언트 시각적 효과
}

void ADRCharacter::OnRep_CarriedPart()
{
	// 클라이언트 시각적 효과
}

void ADRCharacter::InitAbilityActorInfo()
{
	// PlayerState 유효성 검사
	ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();
	if (!DRPlayerState) return;

	// AbilitySystemComponent null 체크
	UAbilitySystemComponent* ASC = DRPlayerState->GetAbilitySystemComponent();
	if (!ASC) return;

	// GAS 컴포넌트들을 PlayerState에서 가져와 초기화
	ASC->InitAbilityActorInfo(DRPlayerState, this);
	Cast<UDRAbilitySystemComponent>(ASC)->AbilityActorInfoSet();

	AbilitySystemComponent = ASC;
	AttributeSet = DRPlayerState->GetAttributeSet();

	// 플레이어 AttributeSet에 컨테이너 정보 설정
	if (UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(AttributeSet))
	{
		PlayerAS->SetContainerInfo(NumContainers, ContainerHealth);
	}

	// ASC 등록 완료 이벤트 브로드캐스트
	OnAscRegistered.Broadcast(AbilitySystemComponent);

	// GAS 태그 바인딩
	AbilitySystemComponent->RegisterGameplayTagEvent(
		FDRGameplayTags::Get().Debuff_Stun,
		EGameplayTagEventType::NewOrRemoved
	).AddUObject(this, &ADRCharacter::StunTagChanged);

	// 플레이어 컨트롤러에 HUD 초기화 요청
	if (ADRPlayerController* DRPlayerController = Cast<ADRPlayerController>(GetController()))
	{
		if (ADRHUD* DRHUD = Cast<ADRHUD>(DRPlayerController->GetHUD()))
		{
			DRHUD->InitOverlay(DRPlayerController, DRPlayerState, AbilitySystemComponent, AttributeSet);
		}
	}

	// 기본 속성 초기화
	InitializeDefaultAttributes();
}
