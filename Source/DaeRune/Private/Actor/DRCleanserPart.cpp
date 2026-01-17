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

	MulticastPlayPickupSound();

	// 캐릭터에게 태그 부착
	UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(CarryingCharacter->GetAbilitySystemComponent());
	if (DRASC)
	{
		DRASC->AddLooseGameplayTag(FDRGameplayTags::Get().State_Carrying);
	}
}

void ADRCleanserPart::InstallPart()
{
	if (!HasAuthority()) return;

	// 캐릭터에게 태그 제거
	UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(CarryingCharacter->GetAbilitySystemComponent());
	if (DRASC)
	{
		DRASC->RemoveLooseGameplayTag(FDRGameplayTags::Get().State_Carrying);
	}
	
	// 액터 파괴
	Destroy();
}

void ADRCleanserPart::DropFromCarrier()
{
	// 서버에서만 실행
	if (!HasAuthority()) return;

	// 캐릭터에서 분리
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// 내부 상태 초기화
	bIsCarried = false;
	CarryingCharacter = nullptr;

	SetActorRotation(FRotator::ZeroRotator);

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
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDRSoundManager* SM = GI->GetSubsystem<UDRSoundManager>())
		{
			SM->PlayPartPickupSound(GetActorLocation());
		}
	}
}

void ADRCleanserPart::MulticastShowInteractionUI_Implementation(ADRPlayerController* PlayerController, bool bShow)
{
	// 모든 클라이언트에서 실행됨
	if (!PlayerController) return;

	// 해당 플레이어의 로컬 컨트롤러에서만 UI 표시/숨김
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
	// 이미 들려있으면 무시
	if (bIsCarried) return;

	ADRCharacter* Character = Cast<ADRCharacter>(OtherActor);
	if (!Character) return;

	// 이미 부품을 들고 있으면 무시
	if (Character->IsCarryingPart()) return;

	ADRPlayerController* PC = Cast<ADRPlayerController>(Character->GetController());
	if (!PC) return;

	// 로컬 컨트롤러에서만 라인트레이싱 활성화
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

	// 로컬 컨트롤러에서만 라인트레이싱 비활성화
	if (PC->IsLocalController())
	{
		PC->SetPartDetectionEnabled(false, this);
	}
}

void ADRCleanserPart::OnRep_bIsCarried()
{
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
	}
	// 부품을 떨어트릴 때
	else
	{
		// 캐릭터에서 분리
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		SetActorRotation(FRotator::ZeroRotator);

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
