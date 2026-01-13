// Copyright DaeRune


#include "Actor/DRCleanserSite.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRCleanserSiteAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectTypes.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Player/DRPlayerController.h"
#include "Character/DRCharacter.h"

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
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UDRCleanserSiteAttributeSet>(TEXT("AttributeSet"));

	// 초기 상태
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

	// 메시 교체
	if (CleanserMesh && CleanserMesh_AfterParts)
	{
		CleanserMesh->SetStaticMesh(CleanserMesh_AfterParts);
	}

	if (WaterMesh && WaterMesh_AfterParts)
	{
		WaterMesh->SetStaticMesh(WaterMesh_AfterParts);
	}
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

	// 설치 개수 증가
	InstalledPartsCount++;

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

FVector ADRCleanserSite::GetSpawnLocation() const
{
	return GetActorLocation();
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

	// 체력 비율을 0~1 사이로 제한
	HealthRatio = FMath::Clamp(HealthRatio, 0.0f, 1.0f);

	// 새 스케일 계산
	FVector NewScale = InitialWaterMeshScale;
	NewScale.Z = InitialWaterMeshScale.Z * HealthRatio;

	// 새 위치 계산
	const float ScaleChange = InitialWaterMeshScale.Z - NewScale.Z;
	const float LocationOffset = ScaleChange * 250.0f;
	FVector NewLocation = InitialWaterMeshLocation;
	NewLocation.Z = InitialWaterMeshLocation.Z + LocationOffset;

	// 적용
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

	// 오버랩 이벤트 바인딩
	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ADRCleanserSite::OnBoxBeginOverlap);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ADRCleanserSite::OnBoxEndOverlap);

	if (!HasAuthority())
	{
		UpdateMeshByState();
	}
}

void ADRCleanserSite::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Phase2가 아니면 무시
	if (CurrentState != ECleanserSiteState::Active) return;

	// 이미 부품이 다 설치되었으면 무시
	if (InstalledPartsCount >= RequiredPartsCount) return;

	ADRCharacter* Character = Cast<ADRCharacter>(OtherActor);
	if (!Character) return;

	// 플레이어가 부품을 들고 있는지 확인
	if (!Character->IsCarryingPart()) return;

	ADRPlayerController* PC = Cast<ADRPlayerController>(Character->GetController());
	if (!PC) return;

	// 로컬 컨트롤러에서만 처리
	if (PC->IsLocalController())
	{
		// PlayerController에 현재 사이트 설정
		PC->CurrentOverlappedSite = this;
		
		// UI 표시
		InteractionWidget->SetVisibility(true);
	}
}

void ADRCleanserSite::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ADRCharacter* Character = Cast<ADRCharacter>(OtherActor);
	if (!Character) return;

	ADRPlayerController* PC = Cast<ADRPlayerController>(Character->GetController());
	if (!PC) return;

	// 로컬 컨트롤러에서만 처리
	if (PC->IsLocalController())
	{
		// PlayerController의 사이트 참조 제거
		PC->CurrentOverlappedSite = nullptr;
		
		// UI 숨김
		InteractionWidget->SetVisibility(false);
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
	// 클라이언트에서 시각적 업데이트
	// 예: 부품 개수에 따라 메시나 이펙트 변경
}

void ADRCleanserSite::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass) const
{
	check(IsValid(GetAbilitySystemComponent()));
	check(GameplayEffectClass);
	
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
		break;

	case ECleanserSiteState::Active:
		if (CleanserMesh) CleanserMesh->SetVisibility(true);
		if (WaterMesh) WaterMesh->SetVisibility(true);
		break;

	case ECleanserSiteState::PartsCollected:
	case ECleanserSiteState::Operational:
	case ECleanserSiteState::Completed:
		// 부품 설치 후 메시로 교체
		if (CleanserMesh && CleanserMesh_AfterParts)
		{
			CleanserMesh->SetStaticMesh(CleanserMesh_AfterParts);
			CleanserMesh->SetVisibility(true);
		}
		if (WaterMesh && WaterMesh_AfterParts)
		{
			WaterMesh->SetStaticMesh(WaterMesh_AfterParts);
			WaterMesh->SetVisibility(true);
		}
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
