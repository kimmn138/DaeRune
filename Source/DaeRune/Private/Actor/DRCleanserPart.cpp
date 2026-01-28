// Copyright DaeRune


#include "Actor/DRCleanserPart.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
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

	MulticastPlayPickupSound();

	// ĳ���Ϳ��� �±� ����
	UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(CarryingCharacter->GetAbilitySystemComponent());
	if (DRASC)
	{
		DRASC->AddLooseGameplayTag(FDRGameplayTags::Get().State_Carrying);
	}
}

void ADRCleanserPart::InstallPart()
{
	if (!HasAuthority()) return;

	// ĳ���Ϳ��� �±� ����
	UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(CarryingCharacter->GetAbilitySystemComponent());
	if (DRASC)
	{
		DRASC->RemoveLooseGameplayTag(FDRGameplayTags::Get().State_Carrying);
	}
	
	// ���� �ı�
	Destroy();
}

void ADRCleanserPart::DropFromCarrier()
{
	// ���������� ����
	if (!HasAuthority()) return;

	// ĳ���Ϳ��� �и�
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// ���� ���� �ʱ�ȭ
	bIsCarried = false;
	CarryingCharacter = nullptr;

	SetActorRotation(FRotator::ZeroRotator);

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

void ADRCleanserPart::MulticastShowInteractionUI_Implementation(ADRPlayerController* PlayerController, bool bShow)
{
	// ��� Ŭ���̾�Ʈ���� �����
	if (!PlayerController) return;

	// �ش� �÷��̾��� ���� ��Ʈ�ѷ������� UI ǥ��/����
	if (PlayerController->IsLocalController())
	{
		InteractionWidget->SetVisibility(bShow);
	}
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
	// �̹� ��������� ����
	if (bIsCarried) return;

	ADRCharacter* Character = Cast<ADRCharacter>(OtherActor);
	if (!Character) return;

	// �̹� ��ǰ�� ��� ������ ����
	if (Character->IsCarryingPart()) return;

	ADRPlayerController* PC = Cast<ADRPlayerController>(Character->GetController());
	if (!PC) return;

	// ���� ��Ʈ�ѷ������� ����Ʈ���̽� Ȱ��ȭ
	if (PC->IsLocalController())
	{
		PC->SetPartDetectionEnabled(true, this);
	}
}

void ADRCleanserPart::OnDetectionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ADRCharacter* Character = Cast<ADRCharacter>(OtherActor);
	if (!Character) return;

	ADRPlayerController* PC = Cast<ADRPlayerController>(Character->GetController());
	if (!PC) return;

	// ���� ��Ʈ�ѷ������� ����Ʈ���̽� ��Ȱ��ȭ
	if (PC->IsLocalController())
	{
		PC->SetPartDetectionEnabled(false, this);
	}
}

void ADRCleanserPart::OnRep_bIsCarried()
{
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
	}
	// ��ǰ�� ����Ʈ�� ��
	else
	{
		// ĳ���Ϳ��� �и�
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		SetActorRotation(FRotator::ZeroRotator);

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
