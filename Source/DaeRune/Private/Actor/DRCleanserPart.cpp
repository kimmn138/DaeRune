// Copyright DaeRune


#include "Actor/DRCleanserPart.h"
#include "Character/DRCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Components/SphereComponent.h"
#include "Player/DRPlayerController.h"
#include "Player/DRInteractionComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/DRSoundDataAsset.h"
#include "DRAssetManager.h"

ADRCleanserPart::ADRCleanserPart()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// 루트 컴포넌트로 부품 메시 생성
	PartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PartMesh"));
	RootComponent = PartMesh;

	// 콜리전 설정
	PartMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PartMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PartMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // 라인트레이싱용

	// 감지 범위 Sphere 생성
	DetectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionSphere"));
	DetectionSphere->SetupAttachment(RootComponent);
	DetectionSphere->SetSphereRadius(DetectionRadius);
	DetectionSphere->bMultiBodyOverlap = false;
	DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// UI 위젯 생성
	InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
	InteractionWidget->SetupAttachment(RootComponent);
	InteractionWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionWidget->SetDrawSize(FVector2D(300.0f, 100.0f));
	InteractionWidget->SetVisibility(false);
	InteractionWidget->SetOwnerNoSee(false);

	// 초기 상태
	bIsCarried = false;
	CarryingCharacter = nullptr;
}

bool ADRCleanserPart::CanBePickedUp() const
{
	return !bIsCarried;
}

void ADRCleanserPart::PickupPart(ADRCharacter* Character)
{
	if (!HasAuthority() || !Character) return;

	// 이미 들려있으면 무시
	if (bIsCarried) return;

	// 상태 업데이트
	bIsCarried = true;
	CarryingCharacter = Character;
	SetOwner(Character);

	// 손 소켓에 붙이고 바닥용 메시 · 콜리전 · UI 를 끈다 (클라는 OnRep 이 같은 함수를 부른다)
	RefreshCarriedState();

	MulticastPlayPickupSound();

	// State.Carrying 태그 토글은 ADRCharacter::SetCarryingState 에서 일괄 처리

	// 획득 알림 (Plan6 §5.6) - 스테이지2 방2가 금고 부품 획득을 감지한다
	OnPartPickedUp.Broadcast(this, Character);
}

void ADRCleanserPart::OnRep_CarryingCharacter()
{
	// bIsCarried 가 먼저 와서 들고 있는 사람을 몰랐던 경우 여기서 마저 맞춘다
	RefreshCarriedState();
	RefreshFirstPersonPart();
}

void ADRCleanserPart::RefreshFirstPersonPart() const
{
	// 들고 있는 사람 화면의 1인칭 부품
	if (bIsCarried && CarryingCharacter)
	{
		CarryingCharacter->ShowFirstPersonPart(PartMesh->GetStaticMesh());
	}
}

void ADRCleanserPart::RefreshCarriedState()
{
	if (bIsCarried)
	{
		PartMesh->SetVisibility(false);
		PartMesh->SetHiddenInGame(true);
		PartMesh->SetOwnerNoSee(false);
		PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DetectionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		InteractionWidget->SetVisibility(false);

		if (CarryingCharacter)
		{
			if (USkeletalMeshComponent* CharacterMesh = CarryingCharacter->GetMesh())
			{
				AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);
			}
		}
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetActorRotation(FRotator::ZeroRotator);

		PartMesh->SetHiddenInGame(false);
		PartMesh->SetVisibility(true);
		PartMesh->SetOwnerNoSee(false);
		PartMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PartMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		PartMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

		DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		DetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
}

void ADRCleanserPart::InstallPart()
{
	if (!HasAuthority()) return;

	// 1인칭 부품 메시 숨기기
	if (CarryingCharacter)
	{
		CarryingCharacter->HideFirstPersonPart();
		CarryingCharacter->HideThirdPersonPart();
	}

	// State.Carrying 태그 해제는 ADRCharacter::SetCarryingState 에서 일괄 처리

	// 액터 파괴
	Destroy();
}

void ADRCleanserPart::DropFromCarrier()
{
	// 서버에서만 실행
	if (!HasAuthority()) return;

	// 1인칭 부품 메시 숨기기 (드롭 전에 캐릭터 참조가 유효한 시점)
	if (CarryingCharacter)
	{
		CarryingCharacter->HideFirstPersonPart();
		CarryingCharacter->HideThirdPersonPart();
	}

	// 내부 상태 초기화 → 분리 · 바닥 메시 · 콜리전 · 감지 범위 복원 (클라는 OnRep 이 같은 함수를 부른다)
	bIsCarried = false;
	CarryingCharacter = nullptr;
	SetOwner(nullptr);
	RefreshCarriedState();
}

void ADRCleanserPart::MulticastPlayPickupSound_Implementation()
{
	UDRSoundDataAsset::PlayAtLocation(this, &UDRSoundDataAsset::PartPickupSound, GetActorLocation());
}

void ADRCleanserPart::SetInteractionUIVisible(bool bShow)
{
	InteractionWidget->SetVisibility(bShow);
}

void ADRCleanserPart::BeginPlay()
{
	Super::BeginPlay();

	DetectionSphere->OnComponentBeginOverlap.AddDynamic(this, &ADRCleanserPart::OnDetectionSphereBeginOverlap);
	DetectionSphere->OnComponentEndOverlap.AddDynamic(this, &ADRCleanserPart::OnDetectionSphereEndOverlap);
}

void ADRCleanserPart::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRCleanserPart, bIsCarried);
	DOREPLIFETIME(ADRCleanserPart, CarryingCharacter);
}

void ADRCleanserPart::OnDetectionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	RefreshOverlapStateFor(Cast<ADRCharacter>(OtherActor));
}

void ADRCleanserPart::OnDetectionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	RefreshOverlapStateFor(Cast<ADRCharacter>(OtherActor));
}

void ADRCleanserPart::RefreshOverlapStateFor(ADRCharacter* Character)
{
	if (!Character || !DetectionSphere) return;

	ADRPlayerController* PC = Cast<ADRPlayerController>(Character->GetController());
	if (!PC || !PC->IsLocalController()) return;

	const bool bIsOverlapping = DetectionSphere->IsOverlappingActor(Character);
	const bool bShouldDetect =
		bIsOverlapping &&
		!bIsCarried &&
		!Character->IsCarryingPart();

	PC->GetInteractionComponent()->SetPartDetectionEnabled(bShouldDetect, this);
}
void ADRCleanserPart::OnRep_bIsCarried()
{
	RefreshCarriedState();
	RefreshFirstPersonPart();
}
