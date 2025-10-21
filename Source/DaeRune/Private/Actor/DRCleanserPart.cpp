// Copyright DaeRune


#include "Actor/DRCleanserPart.h"
#include "Character/DRCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Components/SphereComponent.h"
#include "Player/DRPlayerController.h"
#include "Components/WidgetComponent.h"

ADRCleanserPart::ADRCleanserPart()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// 루트 컴포넌트로 부품 메시 생성
	PartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PartMesh"));
	RootComponent = PartMesh;

	// 콜리전 설정 (라인트레이싱에 감지되도록)
	PartMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PartMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PartMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // 라인트레이싱용

	// 감지 범위 Sphere 생성
	DetectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionSphere"));
	DetectionSphere->SetupAttachment(RootComponent);
	DetectionSphere->SetSphereRadius(DetectionRadius);
	DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// UI 위젯 생성  // ← 추가
	InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
	InteractionWidget->SetupAttachment(RootComponent);
	InteractionWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionWidget->SetDrawSize(FVector2D(300.0f, 100.0f));
	InteractionWidget->SetVisibility(false);
	InteractionWidget->SetOwnerNoSee(false);
	InteractionWidget->SetOnlyOwnerSee(true);

	// 초기 상태
	bIsCarried = false;
	CarryingCharacter = nullptr;
	LineTracingPlayerController = nullptr;
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
}

void ADRCleanserPart::InstallPart()
{
	if (!HasAuthority()) return;

	// 액터 파괴
	Destroy();
}

void ADRCleanserPart::BeginPlay()
{
	Super::BeginPlay();

	// 서버에서만 오버랩 이벤트 바인딩
	if (HasAuthority())
	{
		DetectionSphere->OnComponentBeginOverlap.AddDynamic(this, &ADRCleanserPart::OnDetectionSphereBeginOverlap);
		DetectionSphere->OnComponentEndOverlap.AddDynamic(this, &ADRCleanserPart::OnDetectionSphereEndOverlap);
	}
}

void ADRCleanserPart::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LineTracingPlayerController = nullptr;

	Super::EndPlay(EndPlayReason);
}

void ADRCleanserPart::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRCleanserPart, bIsCarried);
	DOREPLIFETIME(ADRCleanserPart, CarryingCharacter);
}

void ADRCleanserPart::OnDetectionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	// 이미 들려있으면 무시
	if (bIsCarried) return;

	ADRCharacter* Character = Cast<ADRCharacter>(OtherActor);
	if (!Character) return;

	// 이미 부품을 들고 있으면 무시
	if (Character->IsCarryingPart()) return;

	ADRPlayerController* PC = Cast<ADRPlayerController>(Character->GetController());
	if (!PC) return;

	// Controller에게 라인트레이싱 활성화 알림
	PC->SetPartDetectionEnabled(true, this);
}

void ADRCleanserPart::OnDetectionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority()) return;

	ADRCharacter* Character = Cast<ADRCharacter>(OtherActor);
	if (!Character) return;

	ADRPlayerController* PC = Cast<ADRPlayerController>(Character->GetController());
	if (!PC) return;

	// Controller에게 라인트레이싱 비활성화 알림
	PC->SetPartDetectionEnabled(false, this);
}

void ADRCleanserPart::OnLineTraceDetected(ADRPlayerController* PlayerController)
{
	if (!HasAuthority() || !PlayerController) return;

	// 이미 다른 플레이어가 보고 있으면 무시
	if (LineTracingPlayerController) return;

	LineTracingPlayerController = PlayerController;

	// 해당 플레이어의 캐릭터 가져오기
	ADRCharacter* Character = PlayerController->GetPawn<ADRCharacter>();
	if (!Character) return;

	// Owner 설정 및 UI 표시만!
	SetOwner(Character);
	InteractionWidget->SetVisibility(true);
}

void ADRCleanserPart::OnLineTraceLost(ADRPlayerController* PlayerController)
{
	if (!HasAuthority() || !PlayerController) return;

	// 현재 보고 있는 플레이어가 아니면 무시
	if (LineTracingPlayerController != PlayerController) return;

	// UI 숨김만!
	InteractionWidget->SetVisibility(false);
	SetOwner(nullptr);

	LineTracingPlayerController = nullptr;
}

void ADRCleanserPart::OnRep_bIsCarried()
{
	// 클라이언트에서 시각적 업데이트
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
}
