// Copyright DaeRune


#include "Actor/DRCleanserPart.h"
#include "Character/DRCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Components/SphereComponent.h"
#include "Player/DRPlayerController.h"
#include "Components/WidgetComponent.h"
#include "Sound/DRSoundManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/DRSoundDataAsset.h"
#include "DRAssetManager.h"

ADRCleanserPart::ADRCleanserPart()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// ��Ʈ ������Ʈ�� ��ǰ �޽� ����
	PartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PartMesh"));
	RootComponent = PartMesh;

	// �ݸ��� ����
	PartMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PartMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PartMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // ����Ʈ���̽̿�

	// ���� ���� Sphere ����
	DetectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionSphere"));
	DetectionSphere->SetupAttachment(RootComponent);
	DetectionSphere->SetSphereRadius(DetectionRadius);
	DetectionSphere->bMultiBodyOverlap = false;
	DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// UI ���� ����
	InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
	InteractionWidget->SetupAttachment(RootComponent);
	InteractionWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionWidget->SetDrawSize(FVector2D(300.0f, 100.0f));
	InteractionWidget->SetVisibility(false);
	InteractionWidget->SetOwnerNoSee(false);

	// �ʱ� ����
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

	// �̹� ��������� ����
	if (bIsCarried) return;

	// ���� ������Ʈ
	bIsCarried = true;
	CarryingCharacter = Character;

	// ĳ���� �޽� ��������
	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	if (!CharacterMesh) return;

	// �ݸ��� ��Ȱ��ȭ (��� �ִ� ���� �浹 ����)
	PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetectionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// UI ����
	InteractionWidget->SetVisibility(false);

	// ĳ���� ���Ͽ� ����
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

	// ���� �ı�
	Destroy();
}

void ADRCleanserPart::DropFromCarrier()
{
	// ���������� ����
	if (!HasAuthority()) return;

	// 1인칭 부품 메시 숨기기 (드롭 전에 캐릭터 참조가 유효한 시점)
	if (CarryingCharacter)
	{
		CarryingCharacter->HideFirstPersonPart();
		CarryingCharacter->HideThirdPersonPart();
	}

	// ĳ���Ϳ��� �и�
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// ���� ���� �ʱ�ȭ
	bIsCarried = false;
	CarryingCharacter = nullptr;

	SetActorRotation(FRotator::ZeroRotator);

	// 바닥에 떨어진 부품은 모두에게 보이도록 OwnerNoSee 복원
	PartMesh->SetOwnerNoSee(false);
	SetOwner(nullptr);
	RefreshCarriedState();

	// �޽� �ݸ��� ��Ȱ��ȭ
	PartMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PartMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PartMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// ���� ���� ��Ȱ��ȭ
	DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ADRCleanserPart::MulticastPlayPickupSound_Implementation()
{
	// Actor의 World를 직접 사용해서 사운드 재생 (클라이언트에서 확실히 동작)
	if (UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized()))
	{
		if (UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset())
		{
			if (SoundData->PartPickupSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, SoundData->PartPickupSound, GetActorLocation());
			}
		}
	}
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

	// ��ǰ�� �ֿ� �� Ŭ���̾�Ʈ���� �ð��� ������Ʈ
	if (bIsCarried && CarryingCharacter)
	{
		// ĳ���� �޽� ��������
		USkeletalMeshComponent* CharacterMesh = CarryingCharacter->GetMesh();
		if (!CharacterMesh) return;

		// �ݸ��� ��Ȱ��ȭ
		PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DetectionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// UI ����
		InteractionWidget->SetVisibility(false);

		// ĳ���� ���Ͽ� ����
		AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);

		// 3P 부품은 주인에게 안 보이게
		PartMesh->SetOwnerNoSee(true);

		// 1인칭 부품 메시 표시
		CarryingCharacter->ShowFirstPersonPart(PartMesh->GetStaticMesh());
	}
	// ��ǰ�� ����Ʈ�� ��
	else if (!bIsCarried)
	{
		// ĳ���Ϳ��� �и�
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		SetActorRotation(FRotator::ZeroRotator);

		// 바닥에 떨어진 부품은 모두에게 보이도록 복원
		PartMesh->SetOwnerNoSee(false);

		// �޽� �ݸ��� ��Ȱ��ȭ
		PartMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PartMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		PartMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

		// ���� ���� ��Ȱ��ȭ
		DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		DetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
}
