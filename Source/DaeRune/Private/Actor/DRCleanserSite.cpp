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

	// 클렌저 메시 생성 (초기: 숨김)
	CleanserMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CleanserMesh"));
	CleanserMesh->SetupAttachment(RootComponent);
	CleanserMesh->SetVisibility(false);
	CleanserMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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

	if (CleanserMesh)
	{
		CleanserMesh->SetVisibility(true);
	}
}

void ADRCleanserSite::DeactivateSite()
{
	if (!HasAuthority()) return;

	CurrentState = ECleanserSiteState::Inactive;

	if (CleanserMesh)
	{
		CleanserMesh->SetVisibility(false);
	}
}

void ADRCleanserSite::SetPartsCollected()
{
	if (!HasAuthority()) return;

	CurrentState = ECleanserSiteState::PartsCollected;
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

void ADRCleanserSite::StartOperation()
{
	if (!HasAuthority()) return;

	CurrentState = ECleanserSiteState::Operational;
	bHealthEnabled = true;

	// 체력 초기화
	InitializeHealth();

	// 체력 변경 감지 바인딩
	if (AbilitySystemComponent && AttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &ADRCleanserSite::OnHealthChanged);
	}
}

void ADRCleanserSite::StopOperation()
{
	if (!HasAuthority()) return;

	bHealthEnabled = false;

	// 체력 비활성화
	DisableHealth();

	// 체력 변경 감지 언바인딩
	if (AbilitySystemComponent && AttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).RemoveAll(this);
	}
}

void ADRCleanserSite::SetCompleted()
{
	if (!HasAuthority()) return;

	CurrentState = ECleanserSiteState::Completed;
}

FVector ADRCleanserSite::GetSpawnLocation() const
{
	return GetActorLocation();
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
		// PlayerController에 현재 사이트 설정 (CleanserPart 방식과 동일!)
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

void ADRCleanserSite::UpdateInteractionUI()
{
	// 부품이 다 설치되었으면 UI 숨김
	if (InstalledPartsCount >= RequiredPartsCount)
	{
		InteractionWidget->SetVisibility(false);
	}
}

void ADRCleanserSite::OnRep_CurrentState()
{
	// 클라이언트에서 상태 변경 시 시각 효과 업데이트
	switch (CurrentState)
	{
		case ECleanserSiteState::Inactive:
			if (CleanserMesh) CleanserMesh->SetVisibility(false);
			break;
		case ECleanserSiteState::Active:
		case ECleanserSiteState::PartsCollected:
		case ECleanserSiteState::Operational:
		case ECleanserSiteState::Completed:
			if (CleanserMesh) CleanserMesh->SetVisibility(true);
			break;
	}
}

void ADRCleanserSite::OnRep_InstalledPartsCount()
{
	// 클라이언트에서 시각적 업데이트
	// 예: 부품 개수에 따라 메시나 이펙트 변경
}

void ADRCleanserSite::InitializeHealth()
{
	if (!HasAuthority() || !AttributeSet) return;

	// 체력을 최대치로 초기화
	AttributeSet->SetHealth(AttributeSet->GetMaxHealth());
}

void ADRCleanserSite::DisableHealth()
{
	if (!HasAuthority()) return;

	// 체력을 0으로 설정하여 비활성화 표시
	// (실제로는 체력 시스템을 사용하지 않음)
}

void ADRCleanserSite::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!HasAuthority() || !bHealthEnabled) return;

	float NewHealth = Data.NewValue;

	// 체력이 0이 되면 파괴
	if (NewHealth <= 0.0f)
	{
		// 델리게이트 브로드캐스트
		OnCleanserSiteDestroyed.Broadcast(this);

		// GameMode에 알림 (Phase3에서 처리)
	}
}

void ADRCleanserSite::InitAbilityActorInfo()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}
