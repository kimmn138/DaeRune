// Copyright DaeRune


#include "Actor/DRUpgradeStation.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "Player/DRPlayerController.h"
#include "Game/DRGameInstance.h"
#include "Kismet/GameplayStatics.h"

ADRUpgradeStation::ADRUpgradeStation()
{
	PrimaryActorTick.bCanEverTick = false;

	// 장치 자체는 레벨 배치물이라 복제할 상태가 없다 (상호작용은 전부 로컬)
	bReplicates = false;

	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
	RootComponent = StationMesh;
	StationMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(RootComponent);
	InteractionBox->SetBoxExtent(FVector(150.0f, 150.0f, 100.0f));
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
	InteractionWidget->SetupAttachment(RootComponent);
	InteractionWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionWidget->SetDrawSize(FVector2D(300.0f, 100.0f));
	InteractionWidget->SetVisibility(false);
}

void ADRUpgradeStation::BeginPlay()
{
	Super::BeginPlay();

	// 오버랩은 각 머신 로컬로 판정한다 (업그레이드 화면이 로컬 UI라 서버 권한이 필요 없다)
	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ADRUpgradeStation::OnBoxBeginOverlap);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ADRUpgradeStation::OnBoxEndOverlap);
}

void ADRUpgradeStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearLocalController();

	Super::EndPlay(EndPlayReason);
}

bool ADRUpgradeStation::IsUpgradeSystemUnlocked() const
{
	const UDRGameInstance* GI = Cast<UDRGameInstance>(UGameplayStatics::GetGameInstance(this));
	return GI ? GI->IsUpgradeSystemUnlocked() : false;
}

void ADRUpgradeStation::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	ADRPlayerController* DRPC = Cast<ADRPlayerController>(Character->GetController());
	if (!DRPC || !DRPC->IsLocalController()) return;

	// 이미 다른 로컬 컨트롤러가 잡혀 있으면 무시 (분할 화면 대비)
	if (OverlappingLocalController) return;

	OverlappingLocalController = DRPC;
	OverlappingLocalController->OnInteractPressed.AddDynamic(this, &ADRUpgradeStation::OnLocalInteract);

	InteractionWidget->SetVisibility(true);
	OnLocalPlayerEnteredRange(IsUpgradeSystemUnlocked());
}

void ADRUpgradeStation::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	const ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	if (OverlappingLocalController != Cast<ADRPlayerController>(Character->GetController())) return;

	ClearLocalController();
	OnLocalPlayerLeftRange();
}

void ADRUpgradeStation::OnLocalInteract()
{
	if (!OverlappingLocalController) return;

	// 업그레이드 시스템은 스테이지1 최초 클리어로 해금된다 (Plan2.md 5.1)
	if (!IsUpgradeSystemUnlocked())
	{
		OnInteractBlocked();
		return;
	}

	OverlappingLocalController->OpenUpgradeScreen();
}

void ADRUpgradeStation::ClearLocalController()
{
	if (OverlappingLocalController)
	{
		OverlappingLocalController->OnInteractPressed.RemoveDynamic(this, &ADRUpgradeStation::OnLocalInteract);
		OverlappingLocalController = nullptr;
	}

	if (InteractionWidget)
	{
		InteractionWidget->SetVisibility(false);
	}
}
