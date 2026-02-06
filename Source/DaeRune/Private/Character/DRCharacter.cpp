// Copyright DaeRune


#include "Character/DRCharacter.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/Data/GameBalanceConfig.h"
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
#include "Components/PointLightComponent.h"
#include "Game/DRGameUserSettings.h"
#include "UObject/UObjectIterator.h"
#include "Components/SynthComponent.h"

ADRCharacter::ADRCharacter()
{
	// �̵� �������� ȸ�� ����
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 400.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// ī�޶� �� ����
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->SetRelativeLocation(FVector(30.f, 0.f, 50.f));
	CameraBoom->TargetArmLength = 0.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

	// ����ٴϴ� ī�޶� ����
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 1��Ī �޽� ����
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FollowCamera); 
	FirstPersonMesh->SetOnlyOwnerSee(true); 
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 3인칭 메시 설정
	GetMesh()->SetOwnerNoSee(true);

	// VOIPTalker 컴포넌트 생성
	VOIPTalkerComponent = CreateDefaultSubobject<UDRVOIPTalker>(TEXT("VOIPTalker"));

	// 컨트롤러 회전 설정
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = true;

	// �⺻ ĳ���� Ŭ������ ������Ż����Ʈ
	CharacterClass = ECharacterClass::Elementalist;

	// ��ǰ �ý��� �ʱ�ȭ
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
	InitializeMoveSpeedBinding();
}

void ADRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트에서 GAS 초기화
	InitAbilityActorInfo();
	InitializeMoveSpeedBinding();
}

void ADRCharacter::OnRep_Stunned()
{
	if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(AbilitySystemComponent))
	{
		const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
		// �Է� ���� �±׵� ����
		FGameplayTagContainer BlockedTags;
		BlockedTags.AddTag(GameplayTags.Player_Block_InputHeld);
		BlockedTags.AddTag(GameplayTags.Player_Block_InputPressed);
		BlockedTags.AddTag(GameplayTags.Player_Block_InputReleased);
		if (bIsStunned)
		{
			// ���� ����: �Է� ���� + ���� ����Ʈ Ȱ��ȭ
			DRASC->AddLooseGameplayTags(BlockedTags);
			StunDebuffComponent->Activate();
		}
		else
		{
			// ���� ����: �Է� ���� + ���� ����Ʈ ��Ȱ��ȭ
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

	// ��ǰ ȹ�� ó��
	Part->PickupPart(this);

	// ���� ������Ʈ
	bIsCarryingPart = true;
	CarriedPart = Part;

	// ȹ�� �ð� ���
	LastPartPickupTime = GetWorld()->GetTimeSeconds();

	// PlayerController���� UI ǥ�� ��û
	if (ADRPlayerController* PC = Cast<ADRPlayerController>(GetController()))
	{
		PC->ClientShowPartPickupUI();
	}

	return true;
}

void ADRCharacter::InstallCarriedPart()
{
	if (!HasAuthority() || !bIsCarryingPart || !CarriedPart) return;

	// ��ǰ ��ġ ó��
	CarriedPart->InstallPart();

	// ���� �ʱ�ȭ
	bIsCarryingPart = false;
	CarriedPart = nullptr;
}

void ADRCharacter::DropCarriedPart()
{
	// ���������� ����
	if (!HasAuthority()) return;

	// ��ǰ�� ��� ���� ������ ����
	if (!bIsCarryingPart || !CarriedPart) return;

	// ��ٿ� üũ
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const float TimeSincePickup = CurrentTime - LastPartPickupTime;
	if (TimeSincePickup < PartDropCooldown) return;

	// State_Carrying �±� ����
	if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		DRASC->RemoveLooseGameplayTag(FDRGameplayTags::Get().State_Carrying);
	}

	// ��ǰ���� ��������� ��û
	CarriedPart->DropFromCarrier();

	// ĳ���� ���¸� �ʱ�ȭ
	bIsCarryingPart = false;
	CarriedPart = nullptr;
}

void ADRCharacter::TryRegisterVoiceTalker()
{
	if (APlayerState* PS = GetPlayerState())
	{
		GetWorld()->GetTimerManager().ClearTimer(PlayerStateRegisterTimerHandle);
		RegisterVoiceTalker();
	}
}

void ADRCharacter::RegisterVoiceTalker()
{
	if (VOIPTalkerComponent)
	{
		if (APlayerState* PS = GetPlayerState())
		{
			VOIPTalkerComponent->RegisterWithPlayerState(PS);

			// 거리 감쇠 비활성화 (전역 음성)
			VOIPTalkerComponent->Settings.ComponentToAttachTo = nullptr;
			VOIPTalkerComponent->Settings.AttenuationSettings = nullptr;
			VOIPTalkerComponent->Settings.SourceEffectChain = nullptr;
		}
	}
}

void ADRCharacter::UpdateMeshVisibility()
{
	// ���� �÷��̾����� Ȯ��
	const bool bIsLocalPlayer = IsLocallyControlled();

	if (bIsLocalPlayer)
	{
		// ���� �÷��̾�: 1��Ī �޽� ����, 3��Ī �޽� ����
		if (FirstPersonMesh)
		{
			FirstPersonMesh->SetVisibility(true);
		}
		GetMesh()->SetVisibility(false);
		if (Weapon)
		{
			Weapon->SetVisibility(false);
		}
	}
	else
	{
		// �ٸ� �÷��̾�: 3��Ī �޽� ����, 1��Ī �޽� ����
		if (FirstPersonMesh)
		{
			FirstPersonMesh->SetVisibility(false);
		}
		GetMesh()->SetVisibility(true);
		if (Weapon)
		{
			Weapon->SetVisibility(true);
		}
	}
}

void ADRCharacter::BeginPlay()
{
	Super::BeginPlay();

	// �޽� ���ü� ������Ʈ
	UpdateMeshVisibility();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PlayerStateRegisterTimerHandle,
			this,
			&ADRCharacter::TryRegisterVoiceTalker,
			0.2f,
			true
		);
	}

	// 1��Ī ���� ��� �ϴ� ����Ʈ �߰�
	if (IsLocallyControlled())
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(FollowCamera);
		Light->SetRelativeLocation(FVector(-14.2f, 0.f, 23.5f));
		Light->SetIntensity(1500.f);
		Light->SetAttenuationRadius(300.f);
		Light->SetCastShadows(false);
		Light->SetMobility(EComponentMobility::Movable);
		Light->RegisterComponent();
	}
}

void ADRCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 타이머 정리 (메모리 누수 방지)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerStateRegisterTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ADRCharacter::OnRep_bIsCarryingPart()
{
	// Ŭ���̾�Ʈ �ð��� ȿ��
}

void ADRCharacter::OnRep_CarriedPart()
{
	// Ŭ���̾�Ʈ �ð��� ȿ��
}

void ADRCharacter::InitializeMoveSpeedBinding()
{
	if (!AbilitySystemComponent || !AttributeSet) return;

	if (UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet))
	{
		// 이동 속도 변경 델리게이트 바인딩
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(DRAS->GetMoveSpeedAttribute()).AddUObject(this, &ADRCharacter::OnMoveSpeedChanged);

		// 초기 이동 속도 설정
		GetCharacterMovement()->MaxWalkSpeed = DRAS->GetMoveSpeed();
	}
}

float ADRCharacter::GetMoveSpeed()
{
	if (UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet))
	{
		return DRAS->GetMoveSpeed();
	}

	return Super::GetMoveSpeed();
}

void ADRCharacter::InitAbilityActorInfo()
{
	// PlayerState ��ȿ�� �˻�
	ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();
	if (!DRPlayerState) return;

	// AbilitySystemComponent null üũ
	UAbilitySystemComponent* ASC = DRPlayerState->GetAbilitySystemComponent();
	if (!ASC) return;

	// GAS ������Ʈ���� PlayerState���� ������ �ʱ�ȭ
	ASC->InitAbilityActorInfo(DRPlayerState, this);
	Cast<UDRAbilitySystemComponent>(ASC)->AbilityActorInfoSet();

	AbilitySystemComponent = ASC;
	AttributeSet = DRPlayerState->GetAttributeSet();

	// GameBalanceConfig에서 밸런스 값 적용 (서버에서만)
	if (HasAuthority())
	{
		if (const UGameBalanceConfig* BalanceConfig = UDRAbilitySystemLibrary::GetGameBalanceConfig(this))
		{
			NumContainers = BalanceConfig->PlayerContainer.NumContainers;
			ContainerHealth = BalanceConfig->PlayerContainer.ContainerHealth;
			PartDropCooldown = BalanceConfig->PlayerCombat.PartDropCooldown;
		}
	}

	// �÷��̾� AttributeSet�� �����̳� ���� ����
	if (UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(AttributeSet))
	{
		PlayerAS->SetContainerInfo(NumContainers, ContainerHealth);
	}

	// ASC ��� �Ϸ� �̺�Ʈ ��ε�ĳ��Ʈ
	OnAscRegistered.Broadcast(AbilitySystemComponent);

	// GAS �±� ���ε�
	AbilitySystemComponent->RegisterGameplayTagEvent(
		FDRGameplayTags::Get().Debuff_Stun,
		EGameplayTagEventType::NewOrRemoved
	).AddUObject(this, &ADRCharacter::StunTagChanged);

	// �÷��̾� ��Ʈ�ѷ��� HUD �ʱ�ȭ ��û
	if (ADRPlayerController* DRPlayerController = Cast<ADRPlayerController>(GetController()))
	{
		if (ADRHUD* DRHUD = Cast<ADRHUD>(DRPlayerController->GetHUD()))
		{
			DRHUD->InitOverlay(DRPlayerController, DRPlayerState, AbilitySystemComponent, AttributeSet);
		}
	}

	// �⺻ �Ӽ� �ʱ�ȭ
	InitializeDefaultAttributes();
}
