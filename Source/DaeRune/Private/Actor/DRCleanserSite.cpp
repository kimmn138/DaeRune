// Copyright DaeRune


#include "Actor/DRCleanserSite.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRCleanserSiteAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectTypes.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/Widget/DRBillboardWidgetComponent.h"
#include "Player/DRPlayerController.h"
#include "Character/DRCharacter.h"
#include "Sound/DRSoundManager.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/DRSoundDataAsset.h"
#include "DRAssetManager.h"
#include "UI/Widget/DRUserWidget.h"

ADRCleanserSite::ADRCleanserSite()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// ��Ʈ ������Ʈ ����
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	// Ŭ���� �޽� ����
	CleanserMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CleanserMesh"));
	CleanserMesh->SetupAttachment(RootComponent);
	CleanserMesh->SetVisibility(false);
	CleanserMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CleanserMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	// Ŭ���� �� �޽� ����
	WaterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterMesh"));
	WaterMesh->SetupAttachment(CleanserMesh);
	WaterMesh->SetVisibility(false);
	WaterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WaterMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	// �� �޽� �ʱ� ������ �� ��ġ ����
	InitialWaterMeshScale = FVector(1.0f, 1.0f, 1.0f);
	InitialWaterMeshLocation = FVector::ZeroVector;

	// ��ȣ�ۿ� �ڽ� ����
	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(RootComponent);
	InteractionBox->SetBoxExtent(FVector(200.0f, 200.0f, 100.0f));
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// UI ���� ����
	InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
	InteractionWidget->SetupAttachment(RootComponent);
	InteractionWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionWidget->SetDrawSize(FVector2D(300.0f, 100.0f));
	InteractionWidget->SetVisibility(false);
	InteractionWidget->SetOwnerNoSee(false);

	// GAS ������Ʈ ����
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UDRCleanserSiteAttributeSet>(TEXT("AttributeSet"));

	HealthBar = CreateDefaultSubobject<UDRBillboardWidgetComponent>("HealthBar");
	HealthBar->SetupAttachment(GetRootComponent());

	// 설치된 부품 메시 슬롯 (메시와 위치는 블루프린트에서 설정)
	InstalledPartMesh1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InstalledPartMesh1"));
	InstalledPartMesh1->SetupAttachment(CleanserMesh);
	InstalledPartMesh1->SetVisibility(false);
	InstalledPartMesh1->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InstalledPartMesh2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InstalledPartMesh2"));
	InstalledPartMesh2->SetupAttachment(CleanserMesh);
	InstalledPartMesh2->SetVisibility(false);
	InstalledPartMesh2->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// �ʱ� ����
	CurrentState = ECleanserSiteState::Inactive;
	bHealthEnabled = false;
	InstalledPartsCount = 0;
}

void ADRCleanserSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRCleanserSite, CurrentState);
	DOREPLIFETIME(ADRCleanserSite, bHealthEnabled);
	DOREPLIFETIME(ADRCleanserSite, InstalledPartsCount);
}

UAbilitySystemComponent* ADRCleanserSite::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADRCleanserSite::MulticastPlayInstallSound_Implementation(bool bIsComplete)
{
	// Actor의 World를 직접 사용해서 사운드 재생 (클라이언트에서 확실히 동작)
	if (UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized()))
	{
		if (UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset())
		{
			USoundBase* SoundToPlay = bIsComplete ? SoundData->PartInstallCompleteSound : SoundData->PartInstallSound;
			if (SoundToPlay)
			{
				UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, GetActorLocation());
			}
		}
	}
}

void ADRCleanserSite::MulticastStartOperatingSound_Implementation()
{
	// Actor의 World를 직접 사용해서 사운드 재생 (클라이언트에서 확실히 동작)
	if (UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized()))
	{
		if (UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset())
		{
			if (SoundData->CleanserOperatingSound)
			{
				OperatingSoundComponent = UGameplayStatics::SpawnSoundAtLocation(
					this,
					SoundData->CleanserOperatingSound,
					GetActorLocation(),
					FRotator::ZeroRotator,
					1.0f, 1.0f, 0.0f,
					nullptr, nullptr,
					false
				);
			}
		}
	}
}

void ADRCleanserSite::MulticastStopOperatingSound_Implementation()
{
	if (OperatingSoundComponent)
	{
		OperatingSoundComponent->Stop();
		OperatingSoundComponent = nullptr;
	}
}

void ADRCleanserSite::MulticastShowInstalledPart_Implementation(int32 SlotIndex)
{
	UStaticMeshComponent* TargetSlot = nullptr;

	switch (SlotIndex)
	{
	case 0:
		TargetSlot = InstalledPartMesh1;
		break;
	case 1:
		TargetSlot = InstalledPartMesh2;
		break;
	default:
		return;
	}

	if (TargetSlot)
	{
		TargetSlot->SetVisibility(true);
	}
}

void ADRCleanserSite::ActivateSite()
{
	if (!HasAuthority()) return;

	CurrentState = ECleanserSiteState::Active;

	UpdateMeshByState();
}

void ADRCleanserSite::DeactivateSite()
{
	if (!HasAuthority()) return;

	CurrentState = ECleanserSiteState::Inactive;

	UpdateMeshByState();
}

void ADRCleanserSite::SetPartsCollected()
{
	if (!HasAuthority()) return;

	CurrentState = ECleanserSiteState::PartsCollected;

	// 물 메시 교체
	if (WaterMesh && WaterMesh_AfterParts)
	{
		WaterMesh->SetStaticMesh(WaterMesh_AfterParts);
	}
}

void ADRCleanserSite::InstallPart(ADRCharacter* Character)
{
	if (!HasAuthority() || !Character) return;

	// Phase2�� �ƴϸ� ����
	if (CurrentState != ECleanserSiteState::Active) return;

	// �̹� ��ǰ�� �� ��ġ�Ǿ����� ����
	if (InstalledPartsCount >= RequiredPartsCount) return;

	// ĳ���Ͱ� ��ǰ�� ��� �ִ��� Ȯ��
	if (!Character->IsCarryingPart()) return;

	// ��ǰ ��ġ ó��
	Character->InstallCarriedPart();

	// 설치된 슬롯의 부품 메시 표시 (카운트 증가 전에 호출하여 SlotIndex로 사용)
	MulticastShowInstalledPart(InstalledPartsCount);

	// ��ġ ���� ����
	InstalledPartsCount++;

	bool bIsComplete = (InstalledPartsCount >= RequiredPartsCount);
	MulticastPlayInstallSound(bIsComplete);

	// UI ������Ʈ
	UpdateInteractionUI();

	// ��������Ʈ ��ε�ĳ��Ʈ
	OnPartInstalled.Broadcast(this);

	// ��� ��ǰ�� ��ġ�Ǹ� ���� ����
	if (InstalledPartsCount >= RequiredPartsCount)
	{
		SetPartsCollected();
	}
}

FVector ADRCleanserSite::GetSpawnLocation() const
{
	return GetActorLocation();
}

TArray<FVector> ADRCleanserSite::GetPhase1EnemySpawnLocations() const
{
	return Phase1EnemySpawnOffsets;
}

FVector ADRCleanserSite::GetClosestSurfacePoint(const FVector& FromLocation) const
{
	if (!CleanserMesh)
	{
		return GetActorLocation();
	}
    
	FVector ClosestPoint;
    
	// ���� �ݸ��� ���¿��� ���� ����� �� ���
	if (CleanserMesh->GetClosestPointOnCollision(FromLocation, ClosestPoint))
	{
		return ClosestPoint;
	}
    
	// �����ϸ� ���� ��ġ ��ȯ
	return GetActorLocation();
}

void ADRCleanserSite::UpdateWaterMeshScale(float HealthRatio)
{
	if (!WaterMesh) return;
	if (CurrentState != ECleanserSiteState::PartsCollected) return;

	// ü�� ������ 0~1 ���̷� ����
	HealthRatio = FMath::Clamp(HealthRatio, 0.0f, 1.0f);

	// �� ������ ���
	FVector NewScale = InitialWaterMeshScale;
	NewScale.Z = InitialWaterMeshScale.Z * HealthRatio;

	// �� ��ġ ���
	const float ScaleChange = InitialWaterMeshScale.Z - NewScale.Z;
	const float LocationOffset = ScaleChange * 9.0f;
	FVector NewLocation = InitialWaterMeshLocation;
	NewLocation.Z = InitialWaterMeshLocation.Z + LocationOffset;

	// ����
	WaterMesh->SetRelativeScale3D(NewScale);
	WaterMesh->SetRelativeLocation(NewLocation);
}

void ADRCleanserSite::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		InitAbilityActorInfo();
	}

	if (WaterMesh)
	{
		InitialWaterMeshScale = WaterMesh->GetRelativeScale3D();
		InitialWaterMeshLocation = WaterMesh->GetRelativeLocation();
	}

	// ������ �̺�Ʈ ���ε�
	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ADRCleanserSite::OnBoxBeginOverlap);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ADRCleanserSite::OnBoxEndOverlap);

	if (!HasAuthority())
	{
		UpdateMeshByState();
	}

	if (UDRUserWidget* DRUserWidget = Cast<UDRUserWidget>(HealthBar->GetUserWidgetObject()))
	{
		DRUserWidget->SetWidgetController(this);
	}

	// 어트리뷰트 변화 이벤트 바인딩
	if (const UDRCleanserSiteAttributeSet* DRCSAS = Cast<UDRCleanserSiteAttributeSet>(AttributeSet))
	{
		// 체력 변화 델리게이트 바인딩
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(DRCSAS->GetHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnHealthChanged.Broadcast(Data.NewValue);
			}
		);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(DRCSAS->GetMaxHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnMaxHealthChanged.Broadcast(Data.NewValue);
			}
		);

		// 초기값 브로드캐스트
		OnHealthChanged.Broadcast(DRCSAS->GetHealth());
		OnMaxHealthChanged.Broadcast(DRCSAS->GetMaxHealth());
	}
}

void ADRCleanserSite::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	RefreshOverlapStateFor(Cast<ADRCharacter>(OtherActor));
}

void ADRCleanserSite::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	RefreshOverlapStateFor(Cast<ADRCharacter>(OtherActor));
}

void ADRCleanserSite::RefreshOverlapStateFor(ADRCharacter* Character)
{
	if (!Character || !InteractionBox) return;

	ADRPlayerController* PC = Cast<ADRPlayerController>(Character->GetController());
	if (!PC || !PC->IsLocalController()) return;

	const bool bIsOverlapping = InteractionBox->IsOverlappingActor(Character);
	const bool bShouldDetect =
		bIsOverlapping &&
		CurrentState == ECleanserSiteState::Active &&
		InstalledPartsCount < RequiredPartsCount &&
		Character->IsCarryingPart();

	// 사이트 감지를 토글; 실제 위젯 표시 여부는 PlayerController의 라인트레이스가 결정
	PC->SetSiteDetectionEnabled(bShouldDetect, this);
}

void ADRCleanserSite::MulticastShowInteractionUI_Implementation(ADRPlayerController* PlayerController, bool bShow)
{
	if (!PlayerController) return;

	// 해당 플레이어의 로컬 컨트롤러에서만 UI 표시/숨김
	if (PlayerController->IsLocalController() && InteractionWidget)
	{
		InteractionWidget->SetVisibility(bShow);
	}
}

void ADRCleanserSite::UpdateInteractionUI() const
{
	// ��ǰ�� �� ��ġ�Ǿ����� UI ����
	if (InstalledPartsCount >= RequiredPartsCount)
	{
		InteractionWidget->SetVisibility(false);
	}
}

void ADRCleanserSite::OnRep_CurrentState()
{
	// Ŭ���̾�Ʈ���� ���� ���� �� �޽� ������Ʈ
	UpdateMeshByState();
}

void ADRCleanserSite::OnRep_InstalledPartsCount()
{
	// 레이트 조인 클라이언트를 위한 부품 메시 Visibility 복원
	if (InstalledPartMesh1)
	{
		InstalledPartMesh1->SetVisibility(InstalledPartsCount >= 1);
	}
	if (InstalledPartMesh2)
	{
		InstalledPartMesh2->SetVisibility(InstalledPartsCount >= 2);
	}
}

void ADRCleanserSite::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass) const
{
	check(IsValid(GetAbilitySystemComponent()));
	check(GameplayEffectClass);
	
	// ���ؽ�Ʈ ���� �� �ҽ� ����
	FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponent()->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	
	// ���� ���� �� ���� (���� 1�� ����)
	const FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponent()->MakeOutgoingSpec(GameplayEffectClass, 1.0f, ContextHandle);
	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), GetAbilitySystemComponent());
}

void ADRCleanserSite::InitializeDefaultAttributes() const
{
	// �⺻ ü�� �Ӽ� �ʱ�ȭ
	ApplyEffectToSelf(DefaultPrimaryAttributes);
	ApplyEffectToSelf(DefaultVitalAttributes);
}

void ADRCleanserSite::UpdateMeshByState()
{
	switch (CurrentState)
	{
	case ECleanserSiteState::Inactive:
		if (CleanserMesh) CleanserMesh->SetVisibility(false);
		if (WaterMesh) WaterMesh->SetVisibility(false);
		if (InstalledPartMesh1) InstalledPartMesh1->SetVisibility(false);
		if (InstalledPartMesh2) InstalledPartMesh2->SetVisibility(false);
		break;

	case ECleanserSiteState::Active:
		if (CleanserMesh) CleanserMesh->SetVisibility(true);
		if (WaterMesh) WaterMesh->SetVisibility(true);
		// 이미 설치된 부품 복원 (레이트 조인 대비)
		if (InstalledPartMesh1) InstalledPartMesh1->SetVisibility(InstalledPartsCount >= 1);
		if (InstalledPartMesh2) InstalledPartMesh2->SetVisibility(InstalledPartsCount >= 2);
		break;

	case ECleanserSiteState::PartsCollected:
	case ECleanserSiteState::Operational:
	case ECleanserSiteState::Completed:
		if (CleanserMesh) CleanserMesh->SetVisibility(true);
		if (WaterMesh && WaterMesh_AfterParts)
		{
			WaterMesh->SetStaticMesh(WaterMesh_AfterParts);
			WaterMesh->SetVisibility(true);
		}
		// 모든 부품 슬롯 표시 (이 상태면 2개 모두 설치 완료)
		if (InstalledPartMesh1) InstalledPartMesh1->SetVisibility(true);
		if (InstalledPartMesh2) InstalledPartMesh2->SetVisibility(true);
		break;
	}
}

void ADRCleanserSite::InitAbilityActorInfo()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}
