// Copyright DaeRune


#include "Actor/DRLocalInteractStation.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "Player/DRPlayerController.h"

ADRLocalInteractStation::ADRLocalInteractStation()
{
	PrimaryActorTick.bCanEverTick = false;

	// 장치 자체는 레벨 배치물이라 복제할 상태가 없다 (상호작용은 전부 로컬)
	bReplicates = false;

	// 붙이는 곳(루트 메시)은 파생 생성자의 CreateStationRootMesh 가 정한다
	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetBoxExtent(FVector(150.0f, 150.0f, 100.0f));
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
	InteractionWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionWidget->SetDrawSize(FVector2D(300.0f, 100.0f));
	InteractionWidget->SetVisibility(false);
}

UStaticMeshComponent* ADRLocalInteractStation::CreateStationRootMesh(FName MeshName)
{
	UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(MeshName);
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionBox->SetupAttachment(RootComponent);
	InteractionWidget->SetupAttachment(RootComponent);
	return Mesh;
}

void ADRLocalInteractStation::BeginPlay()
{
	Super::BeginPlay();

	// 오버랩은 각 머신 로컬로 판정한다 (화면이 로컬 UI라 서버 권한이 필요 없다)
	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ADRLocalInteractStation::OnBoxBeginOverlap);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ADRLocalInteractStation::OnBoxEndOverlap);
}

void ADRLocalInteractStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearLocalController();

	Super::EndPlay(EndPlayReason);
}

void ADRLocalInteractStation::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	ADRPlayerController* DRPC = Cast<ADRPlayerController>(Character->GetController());
	if (!DRPC || !DRPC->IsLocalController()) return;

	// 이미 다른 로컬 컨트롤러가 잡혀 있으면 무시 (분할 화면 대비)
	if (OverlappingLocalController) return;

	OverlappingLocalController = DRPC;
	OverlappingLocalController->OnInteractPressed.AddDynamic(this, &ADRLocalInteractStation::OnLocalInteract);

	InteractionWidget->SetVisibility(true);
	OnLocalPlayerEnteredRange(CanUseStation());
}

void ADRLocalInteractStation::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	const ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	if (OverlappingLocalController != Cast<ADRPlayerController>(Character->GetController())) return;

	ClearLocalController();
	OnLocalPlayerLeftRange();
}

void ADRLocalInteractStation::OnLocalInteract()
{
	if (!OverlappingLocalController) return;

	if (!CanUseStation())
	{
		OnInteractBlocked();
		return;
	}

	OpenStationScreen(OverlappingLocalController);
}

void ADRLocalInteractStation::ClearLocalController()
{
	if (OverlappingLocalController)
	{
		OverlappingLocalController->OnInteractPressed.RemoveDynamic(this, &ADRLocalInteractStation::OnLocalInteract);
		OverlappingLocalController = nullptr;
	}

	if (InteractionWidget)
	{
		InteractionWidget->SetVisibility(false);
	}
}
