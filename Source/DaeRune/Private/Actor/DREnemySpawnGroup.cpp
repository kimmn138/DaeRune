// Copyright DaeRune


#include "Actor/DREnemySpawnGroup.h"
#include "Character/DREnemy.h"
#include "Components/BillboardComponent.h"

ADREnemySpawnGroup::ADREnemySpawnGroup()
{
	PrimaryActorTick.bCanEverTick = false;

	// 복제할 상태는 없다. bReplicates 는 클라에서 역할을 SimulatedProxy 로 바꿔 HasAuthority() 서버 판정을 지키려고만 켜 두고
	// (복제 안 하는 레벨 배치 액터는 클라에서도 Authority 다), 처음부터 휴면(DORM_Initial)으로 두어 액터 채널을 열지 않는다.
	bReplicates = true;
	NetDormancy = DORM_Initial;

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
			Enemy->SetActorHiddenInGame(false);
			Enemy->SetActorEnableCollision(true);
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

void ADREnemySpawnGroup::SetPartCarrierEnemy(ADREnemy* Carrier)
{
	// 헤더에서는 ADREnemy 가 전방 선언뿐이라 약포인터 대입을 여기서 한다
	PartCarrierEnemy = Carrier;
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

