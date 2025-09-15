// Copyright DaeRune


#include "PlayLoop/EnemyDummy.h"
#include "Components/StaticMeshComponent.h"

// Sets default values
AEnemyDummy::AEnemyDummy()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"));
	RootComponent = Mesh;

	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->OnClicked.AddDynamic(this, &AEnemyDummy::OnClicked_UFN);

	PrimaryActorTick.bCanEverTick = false;
}

void AEnemyDummy::Die()
{
	OnEnemyDied.Broadcast(this);
	Destroy();
}

// Called when the game starts or when spawned
void AEnemyDummy::BeginPlay()
{
	Super::BeginPlay();
	
}

void AEnemyDummy::OnClicked_UFN(UPrimitiveComponent* Touched, FKey Button)
{
	Die();
}

// Called every frame
void AEnemyDummy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

