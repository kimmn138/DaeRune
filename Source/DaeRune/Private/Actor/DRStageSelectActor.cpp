// Copyright DaeRune


#include "Actor/DRStageSelectActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "Player/DRPlayerController.h"
#include "Game/DRLobbyGameMode.h"

ADRStageSelectActor::ADRStageSelectActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

    // 루트 컴포넌트
    PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
    RootComponent = PortalMesh;
    PortalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 상호작용 박스
    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    InteractionBox->SetupAttachment(RootComponent);
    InteractionBox->SetBoxExtent(FVector(200.0f, 200.0f, 100.0f));
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // UI 위젯
    InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
    InteractionWidget->SetupAttachment(RootComponent);
    InteractionWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
    InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
    InteractionWidget->SetDrawSize(FVector2D(300.0f, 100.0f));
    InteractionWidget->SetVisibility(false);
    InteractionWidget->SetOwnerNoSee(false);
    InteractionWidget->SetOnlyOwnerSee(true);

    OverlappingHostController = nullptr;
}

void ADRStageSelectActor::BeginPlay()
{
	Super::BeginPlay();
	
    // 서버에서만 오버랩 이벤트 처리
    if (HasAuthority())
    {
        InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ADRStageSelectActor::OnBoxBeginOverlap);
        InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ADRStageSelectActor::OnBoxEndOverlap);
    }
}

void ADRStageSelectActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 델리게이트 구독 해제
    if (OverlappingHostController)
    {
        OverlappingHostController->OnInteractPressed.RemoveDynamic(this, &ADRStageSelectActor::OnHostInteract);
        OverlappingHostController = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void ADRStageSelectActor::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character) return;

    ADRPlayerController* DRPC = Cast<ADRPlayerController>(Character->GetController());
    if (!DRPC) return;

    // 호스트만 상호작용 가능
    if (!DRPC->IsLocalController()) return;

    // 이미 다른 호스트가 있으면 무시
    if (OverlappingHostController) return;

    OverlappingHostController = DRPC;

    SetOwner(Character);

    // UI 표시
    InteractionWidget->SetVisibility(true);

    // 델리게이트 구독
    OverlappingHostController->OnInteractPressed.AddDynamic(this, &ADRStageSelectActor::OnHostInteract);
}

void ADRStageSelectActor::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!HasAuthority()) return;

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character) return;

    ADRPlayerController* DRPC = Cast<ADRPlayerController>(Character->GetController());
    if (!DRPC) return;

    // 현재 오버랩 중인 호스트가 아니면 무시
    if (OverlappingHostController != DRPC) return;

    // UI 숨김
    InteractionWidget->SetVisibility(false);

    SetOwner(nullptr);

    // 델리게이트 구독 해제
    OverlappingHostController->OnInteractPressed.RemoveDynamic(this, &ADRStageSelectActor::OnHostInteract);
    OverlappingHostController = nullptr;
}

void ADRStageSelectActor::OnHostInteract()
{
    // 서버로 이동 요청
    ServerRequestTravel();
}

void ADRStageSelectActor::ServerRequestTravel_Implementation()
{
    if (!HasAuthority()) return;

    // 호스트가 범위 안에 있는지 확인
    if (!OverlappingHostController) return;

    // GameMode에 위임
    ADRLobbyGameMode* LobbyGameMode = Cast<ADRLobbyGameMode>(GetWorld()->GetAuthGameMode());
    if (!LobbyGameMode) return;

    LobbyGameMode->TravelToStage(DestinationMapName, OverlappingHostController);
}

