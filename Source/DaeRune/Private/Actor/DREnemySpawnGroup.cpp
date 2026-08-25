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

	// �̸� ��ġ�� ���� ���
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

	// �ߺ� ��� ����
	for (const TWeakObjectPtr<ADREnemy>& Registered : RegisteredEnemies)
	{
		if (Registered.Get() == Enemy) return;
	}

	RegisteredEnemies.Add(Enemy);
	AliveEnemyCount++;

	// �� ��� �̺�Ʈ ���ε�
	Enemy->OnDestroyed.AddDynamic(this, &ADREnemySpawnGroup::OnEnemyDestroyed);
}

void ADREnemySpawnGroup::ActivateAllEnemies()
{
	if (!HasAuthority()) return;

	for (const TWeakObjectPtr<ADREnemy>& WeakEnemy : RegisteredEnemies)
	{
		if (ADREnemy* Enemy = WeakEnemy.Get())
		{
			// �� Ȱ��ȭ (DREnemy�� �� �Լ��� �ִٰ� ����)
			Enemy->SetActorHiddenInGame(false);
			Enemy->SetActorEnableCollision(true);

			// AI Ȱ��ȭ�� DREnemy�� �Լ��� ó��
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

	// ���� ��� delegate
	OnEnemyDied.Broadcast(DeadEnemy);

	CheckAllEnemiesDead();
}

void ADREnemySpawnGroup::CheckAllEnemiesDead()
{
	if (AliveEnemyCount <= 0 && !bAllEnemiesDead)
	{
		bAllEnemiesDead = true;

		// ���� delegate
		OnAllEnemiesDead.Broadcast();
	}
}

void ADREnemySpawnGroup::OnRep_AllEnemiesDead()
{
	if (bAllEnemiesDead)
	{
		// Ŭ���̾�Ʈ���� �ʿ��� ó�� (UI ������Ʈ ��)
	}
}

TArray<ADREnemy*> ADREnemySpawnGroup::GetRegisteredEnemies() const
{
	TArray<ADREnemy*> Out;
	Out.Reserve(RegisteredEnemies.Num());
	for (const TWeakObjectPtr<ADREnemy>& Weak : RegisteredEnemies)
	{
		if (ADREnemy* Enemy = Weak.Get())
		{
			Out.Add(Enemy);
		}
	}
	return Out;
}

