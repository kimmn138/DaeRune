// Copyright DaeRune


#include "Actor/DRCleanserSite.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRCleanserSiteAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectTypes.h"

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

	// GAS 컴포넌트 생성
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UDRCleanserSiteAttributeSet>(TEXT("AttributeSet"));

	// 초기 상태
	CurrentState = ECleanserSiteState::Inactive;
	bHealthEnabled = false;
}

void ADRCleanserSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRCleanserSite, CurrentState);
	DOREPLIFETIME(ADRCleanserSite, bHealthEnabled);
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
