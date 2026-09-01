// Copyright DaeRune


#include "Actor/DRWardrobe.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "Game/DRGameInstance.h"
#include "Kismet/GameplayStatics.h"

ADRWardrobe::ADRWardrobe()
{
	PrimaryActorTick.bCanEverTick = false;

	// 장치 자체는 레벨 배치물이라 복제할 상태가 없다 (상호작용은 전부 로컬)
	bReplicates = false;

	WardrobeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WardrobeMesh"));
	RootComponent = WardrobeMesh;
	WardrobeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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

void ADRWardrobe::BeginPlay()
{
	Super::BeginPlay();

	// 오버랩은 각 머신 로컬로 판정한다 (옷장 화면이 로컬 UI라 서버 권한이 필요 없다)
	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ADRWardrobe::OnBoxBeginOverlap);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ADRWardrobe::OnBoxEndOverlap);
}

void ADRWardrobe::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearLocalController();

	Super::EndPlay(EndPlayReason);
}

bool ADRWardrobe::HasAnySkinAvailable() const
{
	const UDRGameInstance* GI = Cast<UDRGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!GI) return false;

	// "지금 고른 로봇"의 옷만 본다 (옷장 화면의 GetViewedClass 와 같은 기준)
	EPlayerCharacterClass ViewedClass = EPlayerCharacterClass::Gardener;
	if (OverlappingLocalController)
	{
		if (const ADRPlayerState* PS = OverlappingLocalController->GetPlayerState<ADRPlayerState>())
		{
			ViewedClass = PS->GetSelectedPlayerClass();
		}
	}

	return GI->HasAnySkinAvailable(ViewedClass);
}

void ADRWardrobe::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	ADRPlayerController* DRPC = Cast<ADRPlayerController>(Character->GetController());
	if (!DRPC || !DRPC->IsLocalController()) return;

	// 이미 다른 로컬 컨트롤러가 잡혀 있으면 무시 (분할 화면 대비)
	if (OverlappingLocalController) return;

	OverlappingLocalController = DRPC;
	OverlappingLocalController->OnInteractPressed.AddDynamic(this, &ADRWardrobe::OnLocalInteract);

	InteractionWidget->SetVisibility(true);
	OnLocalPlayerEnteredRange(HasAnySkinAvailable());
}

void ADRWardrobe::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	const ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character) return;

	if (OverlappingLocalController != Cast<ADRPlayerController>(Character->GetController())) return;

	ClearLocalController();
	OnLocalPlayerLeftRange();
}

void ADRWardrobe::OnLocalInteract()
{
	if (!OverlappingLocalController) return;

	// 카탈로그에 이 로봇의 옷이 하나도 없으면 빈 화면만 뜬다 — 열지 않고 안내한다.
	// ("기본" 칸만 있는 화면은 아무 것도 할 수 없어 버그처럼 보인다)
	if (!HasAnySkinAvailable())
	{
		OnInteractBlocked();
		return;
	}

	OverlappingLocalController->OpenWardrobeScreen();
}

void ADRWardrobe::ClearLocalController()
{
	if (OverlappingLocalController)
	{
		OverlappingLocalController->OnInteractPressed.RemoveDynamic(this, &ADRWardrobe::OnLocalInteract);
		OverlappingLocalController = nullptr;
	}

	if (InteractionWidget)
	{
		InteractionWidget->SetVisibility(false);
	}
}
