// Copyright DaeRune


#include "PlayLoop/CleanserSite.h"
#include "Character/DREnemy.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"

// Sets default values
ACleanserSite::ACleanserSite()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ACleanserSite::BeginPlay()
{
	Super::BeginPlay();
	SetState(ECleanserState::Flag);
}

void ACleanserSite::InitializeAndSpawn()
{
	SetState(ECleanserState::Flag);
	SpawnEnemiesAround();
}

void ACleanserSite::SetState(ECleanserState NewState)
{
	State = NewState;
	if (State == ECleanserState::Flag && FlagMesh)      Mesh->SetStaticMesh(FlagMesh);
	if (State == ECleanserState::Cylinder && CylinderMesh) Mesh->SetStaticMesh(CylinderMesh);
}

void ACleanserSite::SpawnEnemiesAround()
{
    if (!EnemyClass) return;
    UWorld* W = GetWorld(); if (!W) return;

    Spawned.Reset();
    for (int32 i = 0; i < NumEnemiesToSpawn; i++)
    {
        const float Angle = (360.f / NumEnemiesToSpawn) * i;
        const FVector Offset = UKismetMathLibrary::RotateAngleAxis(FVector(SpawnRadius, 0, 0), Angle, FVector::UpVector);
        const FVector Loc = GetActorLocation() + Offset;

        const FTransform T(FRotator::ZeroRotator, Loc);
        ADREnemy* E = W->SpawnActorDeferred<ADREnemy>(EnemyClass, T, this);
        if (E)
        {
            UGameplayStatics::FinishSpawningActor(E, T);
            Spawned.Add(E);
        }
    }
}

void ACleanserSite::TryClear()
{
    if (Spawned.Num() == 0 && State != ECleanserState::Cylinder)
    {
        SetState(ECleanserState::Cylinder);   // ��� �� ����
        OnSiteCleared.Broadcast(this);        // ������1 �Ϸ� �˸�
    }
}

// Called every frame
void ACleanserSite::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}