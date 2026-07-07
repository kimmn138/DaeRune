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
#include "NiagaraFunctionLibrary.h"
#include "AbilitySystem/Debuff/DebuffNiagaraComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UI/HUD/DRHUD.h"
#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "Actor/DRCleanserPart.h"
#include "Actor/DRCleanserSite.h"
#include "Net/UnrealNetwork.h"
#include "Components/PointLightComponent.h"
#include "Game/DRGameUserSettings.h"
#include "UObject/UObjectIterator.h"
#include "Components/WidgetComponent.h"
#include "Game/DRLobbyGameState.h"
#include "Game/DRTutorialGameMode.h"
#include "Character/DRFacialExpressionComponent.h"
#include "DRAssetManager.h"
#include "Sound/DRSoundDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

ADRCharacter::ADRCharacter()
{
	// �̵� �������� ȸ�� ����
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 400.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// Pawn 간 겹침 시 밀어내기 속도 제한 (튕김 방지)
	GetCharacterMovement()->MaxDepenetrationWithPawn = 10.f;

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

	// 호스트(Listen Server)에서 simulated/autonomous proxy의 AnimBP가 항상 평가되고 본도 갱신되도록 강제.
	// AlwaysTickPose만으로는 본 갱신이 가시성 판정에 게이팅되어 시각적으로 안 보일 수 있음.
	// AlwaysTickPoseAndRefreshBones는 가시성/거리와 무관하게 매 프레임 본까지 새로고침.
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// URO(Update Rate Optimizations) 비활성화 — 거리 기반 tick 스로틀링으로 인한 누락 방지.
	// 코옵 PvE 게임이라 동시 플레이어 수가 적어 성능 영향 미미.
	GetMesh()->bEnableUpdateRateOptimizations = false;

	// 1인칭 부품 메시 생성 (픽업 전에는 비활성)
	FirstPersonPartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonPartMesh"));
	FirstPersonPartMesh->SetupAttachment(FirstPersonMesh, FName("TestPartHand"));
	FirstPersonPartMesh->SetOnlyOwnerSee(true);
	FirstPersonPartMesh->bCastDynamicShadow = false;
	FirstPersonPartMesh->CastShadow = false;
	FirstPersonPartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonPartMesh->SetVisibility(false);

	// 3인칭 부품 메시 생성 (픽업 전에는 비활성)
	ThirdPersonPartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThirdPersonPartMesh"));
	ThirdPersonPartMesh->SetupAttachment(GetMesh(), FName("TestPartHand"));
	ThirdPersonPartMesh->SetOwnerNoSee(true);
	ThirdPersonPartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThirdPersonPartMesh->SetVisibility(false);

	// 컨트롤러 회전 설정
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = true;

	// 표정 컴포넌트 생성
	FacialExpressionComponent = CreateDefaultSubobject<UDRFacialExpressionComponent>(TEXT("FacialExpression"));

	// 부품 시스템 초기화
	bIsCarryingPart = false;
	CarriedPart = nullptr;
}

void ADRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRCharacter, bIsCarryingPart);
	DOREPLIFETIME(ADRCharacter, CarriedPart);
	DOREPLIFETIME(ADRCharacter, PlayerCharacterClass);

	// WaterPump 3P 빔 리플리케이트
	DOREPLIFETIME(ADRCharacter, bWaterPumpActive);
	DOREPLIFETIME_CONDITION(ADRCharacter, WaterPumpBeamEndPoint, COND_SkipOwner);
	DOREPLIFETIME(ADRCharacter, WaterPumpWeaponRange);

	// 사망 몽타주 인덱스 (서버 결정, 모든 클라이언트 동일 인덱스 재생)
	DOREPLIFETIME(ADRCharacter, DeathMontageIndex);
}

void ADRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버에서 GAS 초기화 및 어트리뷰트 적용
	InitAbilityActorInfo();

	// UPlayerCharacterClassInfo 기반으로 어빌리티 부여
	// 튜토리얼 모드에서는 자동 부여를 건너뛴다 (튜토리얼 매니저가 목표별로 순차 부여)
	if (HasAuthority())
	{
		const bool bIsTutorial = GetWorld() && Cast<ADRTutorialGameMode>(GetWorld()->GetAuthGameMode()) != nullptr;
		if (!bIsTutorial)
		{
			UDRAbilitySystemLibrary::GivePlayerStartupAbilities(this, AbilitySystemComponent, PlayerCharacterClass);
		}
	}

	InitializeMoveSpeedBinding();

	// 대기실 상태이면 카메라 자동 관리 비활성화 (Possess로 인한 ViewTarget 자동 전환 방지)
	if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(NewController))
	{
		if (DRPC->bIsInWaitingRoom)
		{
			DRPC->bAutoManageActiveCameraTarget = false;
		}
	}
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

void ADRCharacter::RefreshNearbyInteractions()
{
	if (!IsLocallyControlled()) return;

	TArray<AActor*> Overlapping;
	GetOverlappingActors(Overlapping, ADRCleanserSite::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		if (!IsValid(Actor)) continue;
		if (ADRCleanserSite* Site = Cast<ADRCleanserSite>(Actor))
		{
			Site->RefreshOverlapStateFor(this);
		}
	}

	Overlapping.Reset();
	GetOverlappingActors(Overlapping, ADRCleanserPart::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		if (!IsValid(Actor)) continue;
		if (ADRCleanserPart* Part = Cast<ADRCleanserPart>(Actor))
		{
			Part->RefreshOverlapStateFor(this);
		}
	}
}

bool ADRCharacter::PickupPart(ADRCleanserPart* Part)
{
	if (!HasAuthority() || !Part || bIsCarryingPart) return false;

	// ��ǰ ȹ�� ó��
	Part->PickupPart(this);

	SetCarryingState(true, Part);

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

	SetCarryingState(false);
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

	DoDropCarriedPart();
}

void ADRCharacter::ForceDropCarriedPart()
{
	// 서버 권한에서만 실행
	if (!HasAuthority()) return;

	// 부품을 들고 있지 않으면 무시
	if (!bIsCarryingPart || !CarriedPart) return;

	// 피격으로 인한 강제 드롭은 쿨다운을 무시한다
	DoDropCarriedPart();
}

void ADRCharacter::DoDropCarriedPart()
{
	// ��ǰ���� ��������� ��û
	CarriedPart->DropFromCarrier();

	SetCarryingState(false);
}

void ADRCharacter::SetCarryingState(bool bNewCarrying, ADRCleanserPart* Part)
{
	if (!HasAuthority()) return;

	bIsCarryingPart = bNewCarrying;
	CarriedPart = bNewCarrying ? Part : nullptr;

	// State.Carrying 태그 토글 (서버 측. 클라 측은 OnRep_bIsCarryingPart 에서)
	// LooseGameplayTag는 카운트가 누적되므로 현재 태그 보유 여부를 확인 후 토글
	if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		const FGameplayTag CarryingTag = FDRGameplayTags::Get().State_Carrying;
		const bool bHasTag = DRASC->HasMatchingGameplayTag(CarryingTag);
		if (bNewCarrying && !bHasTag)
		{
			DRASC->AddLooseGameplayTag(CarryingTag);
		}
		else if (!bNewCarrying && bHasTag)
		{
			DRASC->RemoveLooseGameplayTag(CarryingTag);
		}
	}

	RefreshCarriedPartVisuals();

	// 리슨 서버 로컬 캐릭터: OnRep이 호출되지 않으므로 여기서 즉시 재평가
	RefreshNearbyInteractions();
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

void ADRCharacter::ShowFirstPersonPart(UStaticMesh* InPartMesh)
{
	if (!FirstPersonPartMesh || !InPartMesh) return;
	if (!IsLocallyControlled()) return;

	FirstPersonPartMesh->SetStaticMesh(InPartMesh);
	FirstPersonPartMesh->SetVisibility(true);
}

void ADRCharacter::HideFirstPersonPart()
{
	if (!FirstPersonPartMesh) return;

	FirstPersonPartMesh->SetVisibility(false);
	FirstPersonPartMesh->SetStaticMesh(nullptr);
}

void ADRCharacter::ShowThirdPersonPart(UStaticMesh* InPartMesh)
{
	if (!ThirdPersonPartMesh || !InPartMesh) return;

	ThirdPersonPartMesh->SetStaticMesh(InPartMesh);
	ThirdPersonPartMesh->SetVisibility(!IsLocallyControlled());
}

void ADRCharacter::HideThirdPersonPart()
{
	if (!ThirdPersonPartMesh) return;

	ThirdPersonPartMesh->SetVisibility(false);
	ThirdPersonPartMesh->SetStaticMesh(nullptr);
}

void ADRCharacter::RefreshCarriedPartVisuals()
{
	UStaticMesh* CarriedStaticMesh = nullptr;

	if (CarriedPart)
	{
		if (UStaticMeshComponent* PMesh = CarriedPart->GetPartMesh())
		{
			CarriedStaticMesh = PMesh->GetStaticMesh();
		}
	}

	if (CarriedStaticMesh)
	{
		ShowFirstPersonPart(CarriedStaticMesh);
		ShowThirdPersonPart(CarriedStaticMesh);
	}
	else
	{
		HideFirstPersonPart();
		HideThirdPersonPart();
	}
}

void ADRCharacter::SetWaitingRoomVisibility(bool bInWaitingRoom)
{
	if (bInWaitingRoom)
	{
		// 1P 메시 숨기기 (고정 카메라에서 불필요)
		if (FirstPersonMesh)
		{
			FirstPersonMesh->SetVisibility(false);
		}

		// 3P 메시 보이기 (자기 자신도 3인칭으로 보여야 함)
		GetMesh()->SetVisibility(true);
		GetMesh()->SetOwnerNoSee(false);

		if (Weapon)
		{
			Weapon->SetVisibility(true);
			Weapon->SetOwnerNoSee(false);
		}

		// 1인칭 부품 메시도 숨기기 (대기실에서는 불필요)
		if (FirstPersonPartMesh)
		{
			FirstPersonPartMesh->SetVisibility(false);
		}
		if (ThirdPersonPartMesh)
		{
			ThirdPersonPartMesh->SetVisibility(false);
		}

		// 대기실에서는 오버헤드 닉네임 숨김 (WBP_PlayerSlot UI에서 표시)
		SetOverheadWidgetVisibility(false);
	}
	else
	{
		// 일반 FPS 모드 복원
		UpdateMeshVisibility();
		GetMesh()->SetOwnerNoSee(true);
		if (Weapon)
		{
			Weapon->SetOwnerNoSee(true);
		}

		RefreshCarriedPartVisuals();

		// FreeRoam부터 오버헤드 닉네임 표시 복원
		SetOverheadWidgetVisibility(true);
	}
}

void ADRCharacter::SetOverheadWidgetVisibility(bool bVisible)
{
	// BeginPlay에서 1회 수집한 캐시 사용 (호출마다 컴포넌트 검색 방지)
	for (const TObjectPtr<UWidgetComponent>& WidgetComp : CachedOverheadWidgets)
	{
		if (WidgetComp)
		{
			WidgetComp->SetVisibility(bVisible);
		}
	}
}

void ADRCharacter::MulticastTeleportToSlot_Implementation(FVector Location, FRotator Rotation)
{
	// CMC와 무관하게 모든 네트워크 엔드포인트에서 직접 위치/회전 설정
	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::ResetPhysics);

	// 잔여 velocity 초기화
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
	}
}

// ========== 자판기 사운드 동기화 (Plan2.md §3.3) ==========

void ADRCharacter::MulticastPlayVendingCoinShot_Implementation(FVector_NetQuantize Location)
{
	UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized());
	if (!AssetManager) return;

	UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset();
	if (!SoundData || !SoundData->VendingCoinShotSound) return;

	UGameplayStatics::PlaySoundAtLocation(this, SoundData->VendingCoinShotSound, Location);
}

void ADRCharacter::MulticastPlayVendingCapsuleShot_Implementation(uint8 CapsuleTier, FVector_NetQuantize Location)
{
	UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized());
	if (!AssetManager) return;

	UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset();
	if (!SoundData) return;

	// 1) Pop 사운드: 모든 Tier 공통
	if (SoundData->VendingJackpotPopSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SoundData->VendingJackpotPopSound, Location);
	}

	// 2) Tier별 보상 사운드 (Bronze=0은 Pop만 재생)
	USoundBase* TierSound = nullptr;
	switch (CapsuleTier)
	{
	case 1: TierSound = SoundData->VendingGainSilverSound; break;
	case 2: TierSound = SoundData->VendingGainGoldSound;   break;
	default: break;
	}
	if (TierSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, TierSound, Location);
	}
}

void ADRCharacter::MulticastPlayVendingSkillUse_Implementation(uint8 NewStacks)
{
	// 공속 버프 표정 트리거 (스킬 사용 시점에 직접 호출)
	if (FacialExpressionComponent)
	{
		FacialExpressionComponent->SetExpression(EFacialExpression::IncreasedAttackSpeed, 2.f);
	}

	UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized());
	if (!AssetManager) return;

	UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset();
	if (!SoundData || !SoundData->VendingSkillUseSound) return;

	// 1.0(1스택) → 1.6(7스택) 정도가 자연스러움.
	const float Pitch = 1.0f + 0.1f * FMath::Max(0, static_cast<int32>(NewStacks) - 1);

	UAudioComponent* Comp = UGameplayStatics::SpawnSoundAttached(
		SoundData->VendingSkillUseSound,
		GetRootComponent(),
		NAME_None,
		FVector::ZeroVector,
		EAttachLocation::SnapToTarget,
		true);
	if (Comp)
	{
		Comp->SetPitchMultiplier(Pitch);
	}
}

void ADRCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 블루프린트에서 추가된 오버헤드 WidgetComponent 1회 수집 (SetOverheadWidgetVisibility에서 사용)
	{
		TArray<UWidgetComponent*> WidgetComponents;
		GetComponents<UWidgetComponent>(WidgetComponents);
		CachedOverheadWidgets.Reset(WidgetComponents.Num());
		for (UWidgetComponent* WidgetComp : WidgetComponents)
		{
			CachedOverheadWidgets.Add(WidgetComp);
		}
	}

	// �޽� ���ü� ������Ʈ
	UpdateMeshVisibility();

	// 1��Ī ���� ��� �ϴ� ����Ʈ �߰�
	//if (IsLocallyControlled())
	//{
	//	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	//	Light->SetupAttachment(FollowCamera);
	//	Light->SetRelativeLocation(FVector(-14.2f, 0.f, 23.5f));
	//	Light->SetIntensity(1500.f);
	//	Light->SetAttenuationRadius(300.f);
	//	Light->SetCastShadows(false);
	//	Light->SetMobility(EComponentMobility::Movable);
	//	Light->RegisterComponent();
	//}

	// 표정 시스템 초기화 (3P 메시에 적용)
	if (FacialExpressionComponent)
	{
		FacialExpressionComponent->InitializeFaceMaterial(GetMesh());
	}

	// 대기실이면 오버헤드 닉네임 위젯 숨김
	if (UWorld* World = GetWorld())
	{
		ADRLobbyGameState* LGS = World->GetGameState<ADRLobbyGameState>();
		if (LGS && (LGS->GetLobbyState() == ELobbyState::WaitingRoom
				 || LGS->GetLobbyState() == ELobbyState::Transitioning))
		{
			SetOverheadWidgetVisibility(false);
		}
	}
}

void ADRCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 타이머 정리 (메모리 누수 방지)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaterPumpBeamUpdateTimer);
	}

	// 3P 빔 정리
	if (WaterPumpThirdPersonBeam)
	{
		WaterPumpThirdPersonBeam->DeactivateImmediate();
		WaterPumpThirdPersonBeam->DestroyComponent();
		WaterPumpThirdPersonBeam = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ADRCharacter::OnRep_WaterPumpActive()
{
	// 소유 클라이언트는 WaterPump 어빌리티에서 1P 빔을 직접 관리
	// 이 함수는 비소유 클라이언트(+ 리슨 서버 호스트의 수동 호출)에서만 3P 빔을 관리
	if (IsLocallyControlled()) return;

	if (bWaterPumpActive)
	{
		// 빠른 토글로 OnRep이 false 분기를 건너뛰고 들어온 경우 이전 빔 누수 방지
		if (WaterPumpThirdPersonBeam)
		{
			WaterPumpThirdPersonBeam->DeactivateImmediate();
			WaterPumpThirdPersonBeam->DestroyComponent();
			WaterPumpThirdPersonBeam = nullptr;
		}
		GetWorldTimerManager().ClearTimer(WaterPumpBeamUpdateTimer);

		// 3P Niagara 빔 생성
		USkeletalMeshComponent* ThirdPersonMesh = GetMesh();
		if (WaterPumpEffectAsset && ThirdPersonMesh
			&& ThirdPersonMesh->DoesSocketExist(WaterPumpMuzzleSocket))
		{
			WaterPumpThirdPersonBeam = UNiagaraFunctionLibrary::SpawnSystemAttached(
				WaterPumpEffectAsset,
				ThirdPersonMesh,
				WaterPumpMuzzleSocket,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				false
			);

			if (WaterPumpThirdPersonBeam)
			{
				WaterPumpThirdPersonBeam->SetOwnerNoSee(true);
				WaterPumpThirdPersonBeam->SetVectorParameter(
					FName("HitEffectPosition"), WaterPumpBeamEndPoint);
			}
		}

		// 보간 위치 초기화
		WaterPumpBeamDisplayEndPoint = WaterPumpBeamEndPoint;

		// 3P 빔 위치 업데이트 타이머 시작 (30fps)
		GetWorldTimerManager().SetTimer(
			WaterPumpBeamUpdateTimer,
			this,
			&ADRCharacter::UpdateWaterPumpThirdPersonBeam,
			0.033f,
			true
		);

		// 즉시 한 번 업데이트하여 초기 회전/위치 보정 (33ms 갭 제거)
		UpdateWaterPumpThirdPersonBeam();
	}
	else
	{
		// 3P Niagara 빔 파괴
		GetWorldTimerManager().ClearTimer(WaterPumpBeamUpdateTimer);

		if (WaterPumpThirdPersonBeam)
		{
			WaterPumpThirdPersonBeam->DeactivateImmediate();
			WaterPumpThirdPersonBeam->DestroyComponent();
			WaterPumpThirdPersonBeam = nullptr;
		}

		WaterPumpBeamDisplayEndPoint = FVector::ZeroVector;
	}
}

void ADRCharacter::UpdateWaterPumpThirdPersonBeam()
{
	if (!WaterPumpThirdPersonBeam) return;
	if (WaterPumpBeamEndPoint.IsNearlyZero()) return;

	USkeletalMeshComponent* ThirdPersonMesh = GetMesh();
	if (!ThirdPersonMesh) return;

	// 소켓 위치 획득
	const FVector SocketLocation = ThirdPersonMesh->GetSocketLocation(WaterPumpMuzzleSocket);

	// 클라이언트 측 보간 (서버 0.1초 갱신을 30fps로 부드럽게)
	const float InterpSpeed = 15.0f;
	WaterPumpBeamDisplayEndPoint = FMath::VInterpTo(
		WaterPumpBeamDisplayEndPoint,
		WaterPumpBeamEndPoint,
		0.033f,
		InterpSpeed
	);

	// 빔 벡터/길이 계산
	FVector BeamVector = WaterPumpBeamDisplayEndPoint - SocketLocation;
	float BeamLength = BeamVector.Size();
	if (BeamLength <= KINDA_SMALL_NUMBER) return;

	FVector BeamDir = BeamVector / BeamLength;
	const FRotator BeamRotation = BeamDir.Rotation();

	// 1인칭과 같은 방식으로 길이 정규화 (어빌리티가 동기화한 사거리 사용)
	const float NormalizedLength = BeamLength / FMath::Max(WaterPumpWeaponRange, KINDA_SMALL_NUMBER);

	const float XValue = FMath::Clamp(NormalizedLength * 1000.0f, 0.0f, 1000.0f);
	const float ZScale = FMath::Clamp(NormalizedLength * 5.0f, 0.0f, 5.0f);

	// Niagara 업데이트
	WaterPumpThirdPersonBeam->SetWorldLocation(SocketLocation);
	WaterPumpThirdPersonBeam->SetWorldRotation(BeamRotation);

	// 빔 길이 반영
	WaterPumpThirdPersonBeam->SetVectorParameter(
		FName("WaterCannonScale"),
		FVector(0.2f, 0.2f, ZScale)
	);

	// 우선 기존 3인칭 방식 유지
	WaterPumpThirdPersonBeam->SetVectorParameter(
		FName("HitEffectPosition"),
		FVector(XValue, 0.0f, 0.0f)
	);
}

void ADRCharacter::OnRep_bIsCarryingPart()
{
	// 클라이언트(소유 컨트롤러)에서 부품 보유 상태가 바뀐 직후 주변 상호작용 UI 재평가
	RefreshNearbyInteractions();

	// 클라 측 ASC 의 owned tag 컨테이너에 직접 State.Carrying 토글
	// (서버는 SetCarryingState 에서 이미 처리. 클라는 ReplicatedLoose 의
	//  RepNotify 경로가 RegisterGameplayTagEvent 를 100% 보장하지 않아 명시 토글 필요)
	if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		const FGameplayTag CarryingTag = FDRGameplayTags::Get().State_Carrying;
		const bool bHasTag = DRASC->HasMatchingGameplayTag(CarryingTag);
		if (bIsCarryingPart && !bHasTag)
		{
			DRASC->AddLooseGameplayTag(CarryingTag);
		}
		else if (!bIsCarryingPart && bHasTag)
		{
			DRASC->RemoveLooseGameplayTag(CarryingTag);
		}
	}
}

void ADRCharacter::OnRep_CarriedPart()
{
	RefreshCarriedPartVisuals();
}

void ADRCharacter::InitializeDefaultAttributes() const
{
	// UPlayerCharacterClassInfo 데이터 에셋 기반 초기화 (EPlayerCharacterClass 타입)
	UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes(this, PlayerCharacterClass, Level, AbilitySystemComponent);
}

void ADRCharacter::InitializeMoveSpeedBinding()
{
	if (!AbilitySystemComponent || !AttributeSets) return;

	if (UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSets))
	{
		// 기존 바인딩 제거 후 재등록 (중복 방지 — 클래스 변경 시 같은 ASC에 다시 바인딩되므로)
		FOnGameplayAttributeValueChange& MoveSpeedDelegate =
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(DRAS->GetMoveSpeedAttribute());
		MoveSpeedDelegate.RemoveAll(this);
		MoveSpeedDelegate.AddUObject(this, &ADRCharacter::OnMoveSpeedChanged);

		// 초기 이동 속도 설정
		GetCharacterMovement()->MaxWalkSpeed = DRAS->GetMoveSpeed();
	}
}

float ADRCharacter::GetMoveSpeed()
{
	if (UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSets))
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
	AttributeSets = DRPlayerState->GetAttributeSet();

	// PlayerState의 선택된 클래스를 PlayerCharacterClass에 반영
	PlayerCharacterClass = DRPlayerState->GetSelectedPlayerClass();

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
	if (UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(AttributeSets))
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

	// 표정 시스템은 명시적 멀티캐스트(MulticastPlayHitReactFacial /
	// MulticastPlayVendingSkillUse)에서 트리거하므로 태그 바인딩 불필요.

	// �÷��̾� ��Ʈ�ѷ��� HUD �ʱ�ȭ ��û
	if (ADRPlayerController* DRPlayerController = Cast<ADRPlayerController>(GetController()))
	{
		// 대기실이면 HUD 오버레이 초기화 스킵 (FreeRoam 진입 시 별도 호출)
		if (!DRPlayerController->bIsInWaitingRoom)
		{
			if (ADRHUD* DRHUD = Cast<ADRHUD>(DRPlayerController->GetHUD()))
			{
				DRHUD->InitOverlay(DRPlayerController, DRPlayerState, AbilitySystemComponent, AttributeSets);
			}
		}
	}

	// 기본 속성 초기화 (서버에서만 - 클라이언트는 복제로 받음)
	if (HasAuthority())
	{
		InitializeDefaultAttributes();
	}
}

void ADRCharacter::MulticastPlayHitReactFacial_Implementation()
{
	if (FacialExpressionComponent)
	{
		FacialExpressionComponent->OnHitReact();
	}
}

// ========== 사망 애니메이션 (이벤트 기반) ==========

void ADRCharacter::MulticastHandleDeath_Implementation(const FVector& DeathImpulse)
{
	// 서버에서만 사망 몽타주 인덱스 결정.
	// Super 호출 전이어야 OnRep_Dead 수동 호출 시점에 인덱스가 채워져 있음.
	if (HasAuthority() && DeathMontages.Num() > 0)
	{
		DeathMontageIndex = FMath::RandRange(0, DeathMontages.Num() - 1);
	}

	// 베이스 본체 실행:
	//  - bDead = true
	//  - 충돌/이동/물리/Dissolve/표정/디버프 정리
	//  - OnRep_Dead() 수동 호출 → ADRCharacter::OnRep_Dead override가 PlayDeathMontage_Internal 호출
	//  - Mesh VisibilityBasedAnimTickOption = AlwaysTickPoseAndRefreshBones
	Super::MulticastHandleDeath_Implementation(DeathImpulse);

	// BP에서 추가 사망 연출이 필요한 경우의 훅
	K2_OnCharacterDied();
}

void ADRCharacter::OnRep_Dead()
{
	Super::OnRep_Dead();

	if (bDead)
	{
		PlayDeathMontage_Internal();
	}
}

void ADRCharacter::PlayDeathMontage_Internal()
{
	// 중복 재생 방지: 멀티캐스트와 RepNotify가 둘 다 호출돼도 1회만 재생.
	if (bDeathMontagePlayed) return;

	if (DeathMontages.Num() == 0) return;

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp) return;

	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (!AnimInst) return;

	// 서버 인덱스 결정이 아직 안 됐을 경우 0번으로 폴백.
	int32 Idx = DeathMontageIndex;
	if (!DeathMontages.IsValidIndex(Idx))
	{
		Idx = 0;
	}

	UAnimMontage* Montage = DeathMontages[Idx];
	if (!Montage) return;

	// HitReact 같은 다른 몽타주가 진행 중이면 즉시 중단
	AnimInst->StopAllMontages(0.1f);

	const float PlayedLength = AnimInst->Montage_Play(Montage, DeathMontagePlayRate);
	if (PlayedLength > 0.f)
	{
		bDeathMontagePlayed = true;
	}
}
