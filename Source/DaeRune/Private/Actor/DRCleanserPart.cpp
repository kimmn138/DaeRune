// Copyright DaeRune


#include "Actor/DRCleanserPart.h"
#include "Character/DRCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Components/SphereComponent.h"
#include "Player/DRPlayerController.h"
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

	// 캐릭터 메시 가져오기
	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	if (!CharacterMesh) return;

	// 콜리전 비활성화 (들고 있는 동안 충돌 방지)
	PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetectionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// UI 숨김
	InteractionWidget->SetVisibility(false);

	// 캐릭터 소켓에 부착
	AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);

	// 3P 부품은 다른 플레이어에게만 보이도록 설정
	PartMesh->SetVisibility(false);
	PartMesh->SetHiddenInGame(true);
	PartMesh->SetOwnerNoSee(false);
	SetOwner(Character);

	// 캐릭터의 1인칭 부품 메시 활성화
	RefreshCarriedState();

	MulticastPlayPickupSound();

	// State.Carrying 태그 토글은 ADRCharacter::SetCarryingState 에서 일괄 처리

	// 획득 알림 (Plan6 §5.6) - 스테이지2 방2가 금고 부품 획득을 감지한다
	OnPartPickedUp.Broadcast(this, Character);
}

void ADRCleanserPart::OnRep_CarryingCharacter()
{
	RefreshCarriedState();
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

	// 캐릭터에서 분리
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// 내부 상태 초기화
	bIsCarried = false;
	CarryingCharacter = nullptr;

	SetActorRotation(FRotator::ZeroRotator);

	// 바닥에 떨어진 부품은 모두에게 보이도록 OwnerNoSee 복원
	PartMesh->SetOwnerNoSee(false);
	SetOwner(nullptr);
	RefreshCarriedState();

	// 메시 콜리전 재활성화
	PartMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PartMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PartMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// 감지 범위 재활성화
	DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
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

	PC->SetPartDetectionEnabled(bShouldDetect, this);
}
void ADRCleanserPart::OnRep_bIsCarried()
{
	RefreshCarriedState();

	// 부품을 주울 때 클라이언트에서 시각적 업데이트
	if (bIsCarried && CarryingCharacter)
	{
		// 캐릭터 메시 가져오기
		USkeletalMeshComponent* CharacterMesh = CarryingCharacter->GetMesh();
		if (!CharacterMesh) return;

		// 콜리전 비활성화
		PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DetectionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// UI 숨김
		InteractionWidget->SetVisibility(false);

		// 캐릭터 소켓에 부착
		AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);

		// 3P 부품은 주인에게 안 보이게
		PartMesh->SetOwnerNoSee(true);

		// 1인칭 부품 메시 표시
		CarryingCharacter->ShowFirstPersonPart(PartMesh->GetStaticMesh());
	}
	// 부품을 떨어트릴 때
	else if (!bIsCarried)
	{
		// 캐릭터에서 분리
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		SetActorRotation(FRotator::ZeroRotator);

		// 바닥에 떨어진 부품은 모두에게 보이도록 복원
		PartMesh->SetOwnerNoSee(false);

		// 메시 콜리전 재활성화
		PartMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PartMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		PartMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

		// 감지 범위 재활성화
		DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		DetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
}
