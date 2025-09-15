// Copyright DaeRune


#include "PlayLoop/StageManager.h"
#include "PlayLoop/CleanserSite.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AStageManager::AStageManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AStageManager::BeginPlay()
{
	Super::BeginPlay();
	
    TArray<AActor*> Points;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), SpawnPointTag, Points);

    if (!CleanserSiteClass || Points.Num() == 0) return;

    const int32 Pick = FMath::RandRange(0, Points.Num() - 1);
    const FTransform TF = Points[Pick]->GetActorTransform();

    ActiveSite = GetWorld()->SpawnActor<ACleanserSite>(CleanserSiteClass, TF);
    if (ActiveSite)
    {
        ActiveSite->OnSiteCleared.AddDynamic(this, &AStageManager::OnSiteCleared);
        ActiveSite->InitializeAndSpawn();
    }
}

void AStageManager::OnSiteCleared(ACleanserSite*)
{
    OnPhase1Completed.Broadcast();   // → 페이즈 매니저/LevelBP/UI가 듣고 2단계 시작
}

void AStageManager::KillAllEnemies()
{
    if (ActiveSite) ActiveSite->ForceKillAll();
}

// Called every frame
void AStageManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

