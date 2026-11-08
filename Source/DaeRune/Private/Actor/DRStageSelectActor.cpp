// Copyright DaeRune


#include "Actor/DRStageSelectActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Player/DRPlayerController.h"
#include "Player/DRInteractionComponent.h"
#include "Game/DRLobbyGameMode.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "DRInteraction"

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

    // 화면 상호작용 안내 문구 (UDRInteractionComponent 가 띄운다)
    PromptText = LOCTEXT("StageSelect", "스테이지 선택하기");
    HostOnlyPromptText = LOCTEXT("StageSelectHostOnly", "방장만 스테이지를 선택할 수 있습니다");
}

void ADRStageSelectActor::BeginPlay()
{
	Super::BeginPlay();

    // 오버랩은 각 머신에서 받는다 — 화면 안내는 참가자에게도 띄우고, F 바인딩은 호스트만 한다
    InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ADRStageSelectActor::OnBoxBeginOverlap);
    InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ADRStageSelectActor::OnBoxEndOverlap);
}

void ADRStageSelectActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ReleaseLocalController();
    ReleaseHostController();

    Super::EndPlay(EndPlayReason);
}

FText ADRStageSelectActor::GetInteractionPromptText(const ADRCharacter* Interactor, bool& bOutUsable) const
{
    // 호스트 = 이 액터의 권한이 있는 머신(리슨 서버)의 로컬 플레이어
    bOutUsable = HasAuthority();
    return bOutUsable ? PromptText : HostOnlyPromptText;
}

void ADRStageSelectActor::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character) return;

    ADRPlayerController* DRPC = Cast<ADRPlayerController>(Character->GetController());
    if (!DRPC || !DRPC->IsLocalController()) return;

    // 화면 안내 — 호스트 · 참가자 모두
    if (!OverlappingLocalController)
    {
        OverlappingLocalController = DRPC;
        OverlappingLocalController->GetInteractionComponent()->SetStationInRange(this, true);
    }

    // 상호작용은 호스트만 (리슨 서버의 로컬 컨트롤러)
    if (!HasAuthority() || OverlappingHostController) return;

    OverlappingHostController = DRPC;
    OverlappingHostController->OnInteractPressed.AddDynamic(this, &ADRStageSelectActor::OnHostInteract);
}

void ADRStageSelectActor::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!IsPawnInRange(OverlappingLocalController))
    {
        ReleaseLocalController();
    }

    if (!IsPawnInRange(OverlappingHostController))
    {
        ReleaseHostController();
    }
}

bool ADRStageSelectActor::IsPawnInRange(const ADRPlayerController* Controller) const
{
    const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    return Pawn && InteractionBox && InteractionBox->IsOverlappingActor(Pawn);
}

void ADRStageSelectActor::ReleaseLocalController()
{
    if (OverlappingLocalController)
    {
        OverlappingLocalController->GetInteractionComponent()->SetStationInRange(this, false);
        OverlappingLocalController = nullptr;
    }
}

void ADRStageSelectActor::ReleaseHostController()
{
    if (OverlappingHostController)
    {
        OverlappingHostController->OnInteractPressed.RemoveDynamic(this, &ADRStageSelectActor::OnHostInteract);
        OverlappingHostController = nullptr;
    }
}

void ADRStageSelectActor::OnHostInteract()
{
    // 스테이지 선택 화면이 준비돼 있으면 화면을 연다 (출발은 화면에서 → ADRPlayerController::RequestStageTravel).
    // 위젯 클래스나 StageSelectList 가 비어 있으면 예전처럼 DestinationMapName 으로 바로 이동한다.
    if (OverlappingHostController && OverlappingHostController->CanUseStageSelectScreen())
    {
        OverlappingHostController->OpenStageSelectScreen();
        return;
    }

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

#undef LOCTEXT_NAMESPACE
