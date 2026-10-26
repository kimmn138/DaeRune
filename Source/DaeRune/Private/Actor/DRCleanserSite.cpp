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
#include "Actor/DRCleanserPart.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/DRSoundDataAsset.h"
#include "DRAssetManager.h"
#include "UI/Widget/DRUserWidget.h"
#include "DaeRune/DRLogChannels.h"

ADRCleanserSite::ADRCleanserSite()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// 루트 컴포넌트 생성
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	// 클렌저 메시 생성
	CleanserMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CleanserMesh"));
	CleanserMesh->SetupAttachment(RootComponent);
	CleanserMesh->SetVisibility(false);
	CleanserMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CleanserMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	// 클렌저 물 메시 생성
	WaterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterMesh"));
	WaterMesh->SetupAttachment(CleanserMesh);
	WaterMesh->SetVisibility(false);
	WaterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WaterMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	// 물 메시 초기 스케일 및 위치 저장
	InitialWaterMeshScale = FVector(1.0f, 1.0f, 1.0f);
	InitialWaterMeshLocation = FVector::ZeroVector;

	// 상호작용 박스 생성
	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(RootComponent);
	InteractionBox->SetBoxExtent(FVector(200.0f, 200.0f, 100.0f));
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// UI 위젯 생성
	InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
	InteractionWidget->SetupAttachment(RootComponent);
	InteractionWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionWidget->SetDrawSize(FVector2D(300.0f, 100.0f));
	InteractionWidget->SetVisibility(false);
	InteractionWidget->SetOwnerNoSee(false);

	// GAS 컴포넌트 생성
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// 플레이어 소유가 아닌 액터라 GE/큐 전체 복제가 불필요 - 어트리뷰트만 복제되는 Minimal이면 충분
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

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

	// 초기 상태
	CurrentState = ECleanserSiteState::Inactive;
	InstalledPartsCount = 0;
}

void ADRCleanserSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRCleanserSite, CurrentState);
	DOREPLIFETIME(ADRCleanserSite, InstalledPartsCount);
}

UAbilitySystemComponent* ADRCleanserSite::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADRCleanserSite::MulticastPlayInstallSound_Implementation(bool bIsComplete)
{
	UDRSoundDataAsset::PlayAtLocation(this,
		bIsComplete ? &UDRSoundDataAsset::PartInstallCompleteSound : &UDRSoundDataAsset::PartInstallSound,
		GetActorLocation());
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

void ADRCleanserSite::SetPartsCollected()
{
	if (!HasAuthority()) return;

	CurrentState = ECleanserSiteState::PartsCollected;

	// 물 메시 교체 (클라는 OnRep_CurrentState 가 같은 함수를 부른다)
	UpdateMeshByState();
}

void ADRCleanserSite::InstallPart(ADRCharacter* Character)
{
	if (!HasAuthority() || !Character) return;

	// Phase2가 아니면 무시
	if (CurrentState != ECleanserSiteState::Active) return;

	// 이미 부품이 다 설치되었으면 무시
	if (InstalledPartsCount >= RequiredPartsCount) return;

	// 캐릭터가 부품을 들고 있는지 확인
	if (!Character->IsCarryingPart()) return;

	// 부품 설치 처리
	Character->InstallCarriedPart();

	// 설치된 슬롯의 부품 메시 표시 (카운트 증가 전에 호출하여 SlotIndex로 사용)
	MulticastShowInstalledPart(InstalledPartsCount);

	// 설치 개수 증가
	InstalledPartsCount++;

	bool bIsComplete = (InstalledPartsCount >= RequiredPartsCount);
	MulticastPlayInstallSound(bIsComplete);

	// UI 업데이트
	UpdateInteractionUI();

	// 델리게이트 브로드캐스트
	OnPartInstalled.Broadcast(this);

	// 모든 부품이 설치되면 상태 변경
	if (InstalledPartsCount >= RequiredPartsCount)
	{
		SetPartsCollected();
	}
}

ADRCleanserPart* ADRCleanserSite::EjectInstalledPart(TSubclassOf<ADRCleanserPart> PartClass, float EjectOffset)
{
	if (!HasAuthority()) return nullptr;

	// 설치된 부품이 없으면 할 일이 없다
	if (InstalledPartsCount <= 0) return nullptr;

	// 설치 카운트를 되돌리고 표시 메시를 숨긴다
	--InstalledPartsCount;

	if (InstalledPartsCount == 0)
	{
		if (InstalledPartMesh1) InstalledPartMesh1->SetVisibility(false);
	}
	if (InstalledPartsCount <= 1)
	{
		if (InstalledPartMesh2) InstalledPartMesh2->SetVisibility(false);
	}

	OnRep_InstalledPartsCount();
	UpdateInteractionUI();

	// ★설치 완료(PartsCollected) 상태로 남으면 InstallPart 와 상호작용 감지가 Active 를 요구해 재설치가 영영 막힌다★
	// (스테이지2 설치대는 부품 1개라 설치 즉시 완료 상태가 된다 — 방4 재시도 소프트락의 원인)
	if (CurrentState == ECleanserSiteState::PartsCollected && InstalledPartsCount < RequiredPartsCount)
	{
		CurrentState = ECleanserSiteState::Active;
		UpdateMeshByState();
	}

	if (!PartClass)
	{
		UE_LOG(LogDR, Error, TEXT("[CleanserSite] EjectInstalledPart: PartClass 가 지정되지 않아 부품을 되돌리지 못했습니다."));
		return nullptr;
	}

	// 설치대 옆에 부품을 다시 스폰한다
	const FVector EjectLocation = GetActorLocation() + GetActorForwardVector() * EjectOffset;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	return GetWorld()->SpawnActor<ADRCleanserPart>(
		PartClass, EjectLocation, GetActorRotation(), SpawnParams);
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
    
	// 실제 콜리전 형태에서 가장 가까운 점 계산
	if (CleanserMesh->GetClosestPointOnCollision(FromLocation, ClosestPoint))
	{
		return ClosestPoint;
	}
    
	// 실패하면 액터 위치 반환
	return GetActorLocation();
}

void ADRCleanserSite::UpdateWaterMeshScale(float HealthRatio)
{
	if (!WaterMesh) return;
	if (CurrentState != ECleanserSiteState::PartsCollected) return;

	// 체력 비율을 0~1 사이로 제한
	HealthRatio = FMath::Clamp(HealthRatio, 0.0f, 1.0f);

	// 새 스케일 계산
	FVector NewScale = InitialWaterMeshScale;
	NewScale.Z = InitialWaterMeshScale.Z * HealthRatio;

	// 새 위치 계산
	const float ScaleChange = InitialWaterMeshScale.Z - NewScale.Z;
	const float LocationOffset = ScaleChange * WaterMeshScaleToOffsetRatio;
	FVector NewLocation = InitialWaterMeshLocation;
	NewLocation.Z = InitialWaterMeshLocation.Z + LocationOffset;

	// 적용
	WaterMesh->SetRelativeScale3D(NewScale);
	WaterMesh->SetRelativeLocation(NewLocation);
}

void ADRCleanserSite::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// BP · 배치 값이 들어간 뒤, 어떤 상태 전환보다 먼저 물 메시의 처음 모습을 기억한다
	if (WaterMesh)
	{
		InitialWaterMeshScale = WaterMesh->GetRelativeScale3D();
		InitialWaterMeshLocation = WaterMesh->GetRelativeLocation();
		InitialWaterStaticMesh = WaterMesh->GetStaticMesh();
	}
}

void ADRCleanserSite::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		InitAbilityActorInfo();
	}

	// 오버랩 이벤트 바인딩
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
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(DRCSAS->GetHealthAttribute()).AddWeakLambda(
			this, [this](const FOnAttributeChangeData& Data)
			{
				OnHealthChanged.Broadcast(Data.NewValue);
			}
		);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(DRCSAS->GetMaxHealthAttribute()).AddWeakLambda(
			this, [this](const FOnAttributeChangeData& Data)
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

void ADRCleanserSite::SetInteractionUIVisible(bool bShow)
{
	if (InteractionWidget)
	{
		InteractionWidget->SetVisibility(bShow);
	}
}

void ADRCleanserSite::UpdateInteractionUI() const
{
	// 부품이 다 설치되었으면 UI 숨김
	if (InstalledPartsCount >= RequiredPartsCount)
	{
		InteractionWidget->SetVisibility(false);
	}
}

void ADRCleanserSite::OnRep_CurrentState()
{
	// 클라이언트에서 상태 변경 시 메시 업데이트
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
	// BP 에서 기본 속성 GE 를 비워 두면 크래시 대신 경고만 남긴다
	if (!IsValid(GetAbilitySystemComponent()) || !GameplayEffectClass)
	{
		UE_LOG(LogDR, Warning, TEXT("[CleanserSite] %s: ASC 또는 GE 클래스가 없어 속성 효과를 적용하지 못했습니다."), *GetName());
		return;
	}

	// 컨텍스트 생성 및 소스 설정
	FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponent()->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	
	// 스펙 생성 및 적용 (레벨 1로 고정)
	const FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponent()->MakeOutgoingSpec(GameplayEffectClass, 1.0f, ContextHandle);
	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), GetAbilitySystemComponent());
}

void ADRCleanserSite::InitializeDefaultAttributes() const
{
	// 기본 체력 속성 초기화
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
		if (WaterMesh)
		{
			// 부품을 되돌려 설치 완료에서 다시 내려온 경우 물을 처음 모습으로 되돌린다
			if (InitialWaterStaticMesh)
			{
				WaterMesh->SetStaticMesh(InitialWaterStaticMesh);
			}
			WaterMesh->SetRelativeScale3D(InitialWaterMeshScale);
			WaterMesh->SetRelativeLocation(InitialWaterMeshLocation);
			WaterMesh->SetVisibility(true);
		}
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
		break;
	}

	// 설치된 부품 슬롯은 상태와 무관하게 개수로 정한다 (스테이지2 설치대는 부품 1개 — 완료라도 2번 슬롯은 비어 있다)
	if (CurrentState != ECleanserSiteState::Inactive)
	{
		OnRep_InstalledPartsCount();
	}
}

void ADRCleanserSite::InitAbilityActorInfo()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}
