// Copyright DaeRune


#include "Actor/DRStageSelectActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "Player/DRPlayerController.h"
#include "Game/DRLobbyGameMode.h"
#include "TimerManager.h"
#include "Engine/World.h"

ADRStageSelectActor::ADRStageSelectActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

    // ��Ʈ ������Ʈ
    PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
    RootComponent = PortalMesh;
    PortalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // ��ȣ�ۿ� �ڽ�
    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    InteractionBox->SetupAttachment(RootComponent);
    InteractionBox->SetBoxExtent(FVector(200.0f, 200.0f, 100.0f));
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // UI ����
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
	
    // ���������� ������ �̺�Ʈ ó��
    if (HasAuthority())
    {
        InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ADRStageSelectActor::OnBoxBeginOverlap);
        InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ADRStageSelectActor::OnBoxEndOverlap);

        // 호스트는 범위와 무관하게 UI가 항상 보이도록 셋업
        SetupHostWidgetVisibility();
    }
}

void ADRStageSelectActor::SetupHostWidgetVisibility()
{
    if (!HasAuthority()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->IsLocalController()) continue;

        APawn* HostPawn = PC->GetPawn();
        if (!HostPawn) continue;

        HostController = Cast<ADRPlayerController>(PC);
        SetOwner(HostPawn);
        InteractionWidget->SetVisibility(true);
        return;
    }

    // 호스트의 폰이 아직 준비되지 않은 경우 재시도
    World->GetTimerManager().SetTimer(
        HostSetupTimerHandle,
        this,
        &ADRStageSelectActor::SetupHostWidgetVisibility,
        0.5f,
        false
    );
}

void ADRStageSelectActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HostSetupTimerHandle);
    }

    // ��������Ʈ ���� ����
    if (OverlappingHostController)
    {
        OverlappingHostController->OnInteractPressed.RemoveDynamic(this, &ADRStageSelectActor::OnHostInteract);
        OverlappingHostController = nullptr;
    }

    HostController = nullptr;

    Super::EndPlay(EndPlayReason);
}

void ADRStageSelectActor::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority()) return;

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character) return;

    ADRPlayerController* DRPC = Cast<ADRPlayerController>(Character->GetController());
    if (!DRPC) return;

    // ȣ��Ʈ�� ��ȣ�ۿ� ����
    if (!DRPC->IsLocalController()) return;

    // �̹� �ٸ� ȣ��Ʈ�� ������ ����
    if (OverlappingHostController) return;

    OverlappingHostController = DRPC;

    // ��������Ʈ ����
    OverlappingHostController->OnInteractPressed.AddDynamic(this, &ADRStageSelectActor::OnHostInteract);
}

void ADRStageSelectActor::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!HasAuthority()) return;

    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character) return;

    ADRPlayerController* DRPC = Cast<ADRPlayerController>(Character->GetController());
    if (!DRPC) return;

    // ���� ������ ���� ȣ��Ʈ�� �ƴϸ� ����
    if (OverlappingHostController != DRPC) return;

    // ��������Ʈ ���� ����
    OverlappingHostController->OnInteractPressed.RemoveDynamic(this, &ADRStageSelectActor::OnHostInteract);
    OverlappingHostController = nullptr;
}

void ADRStageSelectActor::OnHostInteract()
{
    // ������ �̵� ��û
    ServerRequestTravel();
}

void ADRStageSelectActor::ServerRequestTravel_Implementation()
{
    if (!HasAuthority()) return;

    // ȣ��Ʈ�� ���� �ȿ� �ִ��� Ȯ��
    if (!OverlappingHostController) return;

    // GameMode�� ����
    ADRLobbyGameMode* LobbyGameMode = Cast<ADRLobbyGameMode>(GetWorld()->GetAuthGameMode());
    if (!LobbyGameMode) return;

    LobbyGameMode->TravelToStage(DestinationMapName, OverlappingHostController);
}

