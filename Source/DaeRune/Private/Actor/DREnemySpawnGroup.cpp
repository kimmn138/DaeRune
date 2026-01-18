// Copyright DaeRune


#include "Actor/DREnemySpawnGroup.h"
#include "Character/DREnemy.h"
#include "Components/BillboardComponent.h"
#include "Net/UnrealNetwork.h"

ADREnemySpawnGroup::ADREnemySpawnGroup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	// Root Component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	SetRootComponent(RootSceneComponent);
}

void ADREnemySpawnGroup::BeginPlay()
{
	Super::BeginPlay();

	// 미리 배치된 적들 등록
	if (HasAuthority())
	{
		for (const TSoftObjectPtr<ADREnemy>& SoftEnemy : PrePlacedEnemies)
		{
			if (ADREnemy* Enemy = SoftEnemy.Get())
			{
				RegisterEnemy(Enemy);
			}
		}
	}
}

void ADREnemySpawnGroup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADREnemySpawnGroup, AliveEnemyCount);
	DOREPLIFETIME(ADREnemySpawnGroup, bAllEnemiesDead);
}

void ADREnemySpawnGroup::RegisterEnemy(ADREnemy* Enemy)
{
	if (!HasAuthority()) return;
	if (!Enemy) return;

	// 중복 등록 방지
	for (const TWeakObjectPtr<ADREnemy>& Registered : RegisteredEnemies)
	{
		if (Registered.Get() == Enemy) return;
	}

	RegisteredEnemies.Add(Enemy);
	AliveEnemyCount++;

	// 적 사망 이벤트 바인딩
	Enemy->OnDestroyed.AddDynamic(this, &ADREnemySpawnGroup::OnEnemyDestroyed);
}

void ADREnemySpawnGroup::ActivateAllEnemies()
{
	if (!HasAuthority()) return;

	for (const TWeakObjectPtr<ADREnemy>& WeakEnemy : RegisteredEnemies)
	{
		if (ADREnemy* Enemy = WeakEnemy.Get())
		{
			// 적 활성화 (DREnemy에 이 함수가 있다고 가정)
			Enemy->SetActorHiddenInGame(false);
			Enemy->SetActorEnableCollision(true);

			// AI 활성화는 DREnemy의 함수로 처리
			// Enemy->ActivateAI();
		}
	}
}

void ADREnemySpawnGroup::OnEnemyDestroyed(AActor* DestroyedActor)
{
	if (!HasAuthority()) return;

	ADREnemy* DeadEnemy = Cast<ADREnemy>(DestroyedActor);
	if (!DeadEnemy) return;

	AliveEnemyCount = FMath::Max(0, AliveEnemyCount - 1);

	// 개별 사망 delegate
	OnEnemyDied.Broadcast(DeadEnemy);

	CheckAllEnemiesDead();
}

void ADREnemySpawnGroup::CheckAllEnemiesDead()
{
	if (AliveEnemyCount <= 0 && !bAllEnemiesDead)
	{
		bAllEnemiesDead = true;

		// 전멸 delegate
		OnAllEnemiesDead.Broadcast();
	}
}

void ADREnemySpawnGroup::OnRep_AllEnemiesDead()
{
	if (bAllEnemiesDead)
	{
		// 클라이언트에서 필요한 처리 (UI 업데이트 등)
	}
}

