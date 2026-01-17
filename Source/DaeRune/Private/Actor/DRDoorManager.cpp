// Copyright DaeRune


#include "Actor/DRDoorManager.h"
#include "Actor/DRAutoSlidingDoor.h"
#include "Actor/DRBreakableDoor.h"
#include "Actor/DREnemySpawnGroup.h"
#include "Game/DRStageGameState.h"

ADRDoorManager::ADRDoorManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	// Root Component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	SetRootComponent(RootSceneComponent);
}

void ADRDoorManager::BeginPlay()
{
	Super::BeginPlay();

	// 서버에서만 이벤트 바인딩
	if (HasAuthority())
	{
		// GameState에 자신을 등록
		if (ADRStageGameState* StageGameState = GetWorld()->GetGameState<ADRStageGameState>())
		{
			StageGameState->RegisterDoorManager(this);
		}
		BindDoorEvents();
		BindSpawnGroupEvents();
	}
}

void ADRDoorManager::BindDoorEvents()
{
	// 자동문 닫힘 이벤트 바인딩
	if (AutoDoor_Room1ToRoom6)
	{
		AutoDoor_Room1ToRoom6->OnDoorClosed.AddDynamic(this, &ADRDoorManager::OnAutoDoorClosed);
	}
}

void ADRDoorManager::BindSpawnGroupEvents()
{
	// 방4 스폰 그룹 전멸 이벤트
	if (SpawnGroup_Room4)
	{
		SpawnGroup_Room4->OnAllEnemiesDead.AddDynamic(this, &ADRDoorManager::OnRoom4Cleared);
	}

	// 방5 스폰 그룹 전멸 이벤트
	if (SpawnGroup_Room5)
	{
		SpawnGroup_Room5->OnAllEnemiesDead.AddDynamic(this, &ADRDoorManager::OnRoom5Cleared);
	}
}

void ADRDoorManager::OnAutoDoorClosed()
{
	// 이미 트리거됐으면 무시
	if (bInitialDoorsTriggered) return;
	bInitialDoorsTriggered = true;

	// 딜레이 후 문 파괴
	if (DelayAfterAutoDoorClosed > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			InitialBreakTimerHandle,
			this,
			&ADRDoorManager::BreakInitialDoors,
			DelayAfterAutoDoorClosed,
			false
		);
	}
	else
	{
		BreakInitialDoors();
	}
}

void ADRDoorManager::BreakInitialDoors()
{
	// 방1-방2 문 파괴
	if (BreakableDoor_Room1ToRoom2)
	{
		BreakableDoor_Room1ToRoom2->Break();
	}

	// 방1-방3 문 파괴
	if (BreakableDoor_Room1ToRoom3)
	{
		BreakableDoor_Room1ToRoom3->Break();
	}
}

void ADRDoorManager::OnRoom4Cleared()
{
	// 방4-방6 문 파괴
	if (BreakableDoor_Room4ToRoom6 && !BreakableDoor_Room4ToRoom6->IsBroken())
	{
		BreakableDoor_Room4ToRoom6->Break();
	}
}

void ADRDoorManager::OnRoom5Cleared()
{
	// 방5-방6 문 파괴
	if (BreakableDoor_Room5ToRoom6 && !BreakableDoor_Room5ToRoom6->IsBroken())
	{
		BreakableDoor_Room5ToRoom6->Break();
	}
}

void ADRDoorManager::OnPhase1Ended()
{
	if (!HasAuthority()) return;

	// 자동문 열기
	if (AutoDoor_Room1ToRoom6)
	{
		AutoDoor_Room1ToRoom6->OpenDoor();
	}

	// 아직 안 부서진 문들 파괴
	BreakRemainingDoors();
}

void ADRDoorManager::BreakRemainingDoors()
{
	// 방4-방6 문
	if (BreakableDoor_Room4ToRoom6 && !BreakableDoor_Room4ToRoom6->IsBroken())
	{
		BreakableDoor_Room4ToRoom6->BreakWithoutEnemies();
	}

	// 방5-방6 문
	if (BreakableDoor_Room5ToRoom6 && !BreakableDoor_Room5ToRoom6->IsBroken())
	{
		BreakableDoor_Room5ToRoom6->BreakWithoutEnemies();
	}
}

