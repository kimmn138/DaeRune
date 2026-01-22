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
#include "Components/PointLightComponent.h"
#include "Game/DRGameUserSettings.h"
#include "UObject/UObjectIterator.h"
#include "Components/SynthComponent.h"

ADRCharacter::ADRCharacter()
{
	// 이동 방향으로 회전 설정
	GetCharacterMovement()->bOrientRotationToMovement = false;
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

	// 1인칭 메쉬 설정
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FollowCamera); 
	FirstPersonMesh->SetOnlyOwnerSee(true); 
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 3인칭 메쉬 설정
	GetMesh()->SetOwnerNoSee(true);

	// VOIPTalker 컴포넌트 생성
	//VOIPTalkerComponent = CreateDefaultSubobject<UDRVOIPTalker>(TEXT("VDROIPTalker"));

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

	if (UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet))
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(DRAS->GetMoveSpeedAttribute()).AddUObject(this, &ADRCharacter::OnMoveSpeedChanged);
		GetCharacterMovement()->MaxWalkSpeed = DRAS->GetMoveSpeed();
	}
}

void ADRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트에서 GAS 초기화
	InitAbilityActorInfo();

	if (UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet))
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(DRAS->GetMoveSpeedAttribute()).AddUObject(this, &ADRCharacter::OnMoveSpeedChanged);
		GetCharacterMovement()->MaxWalkSpeed = DRAS->GetMoveSpeed();
	}
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

	// 획득 시간 기록
	LastPartPickupTime = GetWorld()->GetTimeSeconds();

	// PlayerController에게 UI 표시 요청
	if (ADRPlayerController* PC = Cast<ADRPlayerController>(GetController()))
	{
		PC->ClientShowPartPickupUI();
	}

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

void ADRCharacter::DropCarriedPart()
{
	// 서버에서만 실행
	if (!HasAuthority()) return;

	// 부품을 들고 있지 않으면 무시
	if (!bIsCarryingPart || !CarriedPart) return;

	// 쿨다운 체크
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const float TimeSincePickup = CurrentTime - LastPartPickupTime;
	if (TimeSincePickup < PartDropCooldown) return;

	// State_Carrying 태그 제거
	if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		DRASC->RemoveLooseGameplayTag(FDRGameplayTags::Get().State_Carrying);
	}

	// 부품에게 떨어지라고 요청
	CarriedPart->DropFromCarrier();

	// 캐릭터 상태만 초기화
	bIsCarryingPart = false;
	CarriedPart = nullptr;
}

void ADRCharacter::TryRegisterVoiceTalker()
{
	/*if (APlayerState* PS = GetPlayerState())
	{
		GetWorld()->GetTimerManager().ClearTimer(PlayerStateRegisterTimerHanlde);
		RegisterVoiceTalker();
	}*/
}

void ADRCharacter::RegisterVoiceTalker()
{
	//if (VOIPTalkerComponent)
	//{
	//	if (APlayerState* PS = GetPlayerState())
	//	{
	//		VOIPTalkerComponent->RegisterWithPlayerState(PS);

	//		// 거리 감쇠 비활성화
	//		VOIPTalkerComponent->Settings.ComponentToAttachTo = nullptr;
	//		VOIPTalkerComponent->Settings.AttenuationSettings = nullptr;
	//		VOIPTalkerComponent->Settings.SourceEffectChain = nullptr;
	//	}
	//}
}

void ADRCharacter::UpdateMeshVisibility()
{
	// 로컬 플레이어인지 확인
	const bool bIsLocalPlayer = IsLocallyControlled();

	if (bIsLocalPlayer)
	{
		// 로컬 플레이어: 1인칭 메쉬 보임, 3인칭 메쉬 숨김
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
		// 다른 플레이어: 3인칭 메쉬 보임, 1인칭 메쉬 숨김
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

	// 메쉬 가시성 업데이트
	UpdateMeshVisibility();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PlayerStateRegisterTimerHanlde,
			this,
			&ADRCharacter::TryRegisterVoiceTalker,
			0.2f,
			true
		);
	}

	// 1인칭 시점 밝게 하는 라이트 추가
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
	//// VOIPTalker 정리
	//if (VOIPTalkerComponent)
	//{
	//	// 오디오 스트림 즉시 중지
	//	if (VOIPTalkerComponent->IsActive())
	//	{
	//		VOIPTalkerComponent->Deactivate();
	//	}

	//	// 컴포넌트 명시적 파괴
	//	VOIPTalkerComponent->DestroyComponent();
	//}

	/*if (IsLocallyControlled())
	{
		for (TObjectIterator<USynthComponent> It; It; ++It)
		{
			USynthComponent* SynthComp = *It;
			if (SynthComp && SynthComp->GetClass()->GetName().Contains(TEXT("VoipListenerSynthComponent")))
			{
				SynthComp->Stop();
				if (SynthComp->IsRegistered())
				{
					SynthComp->UnregisterComponent();
				}
			}
		}
	}*/

	Super::EndPlay(EndPlayReason);
}

void ADRCharacter::OnRep_bIsCarryingPart()
{
	// 클라이언트 시각적 효과
}

void ADRCharacter::OnRep_CarriedPart()
{
	// 클라이언트 시각적 효과
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
