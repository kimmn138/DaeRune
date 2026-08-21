// Copyright DaeRune

#include "Actor/Stage2/DRS2StageDirector.h"

#include "Actor/Stage2/DRS2PassageBlocker.h"
#include "Actor/Stage2/DRS2TeleportGate.h"
#include "Actor/Stage2/DRS2RoomTrigger.h"
#include "Actor/Stage2/DRS2SlidePuzzle.h"
#include "Actor/Stage2/DRS2SwitchPuzzle.h"
#include "Actor/Stage2/DRS2CctvBoard.h"
#include "Actor/Stage2/DRS2Safe.h"
#include "Actor/Stage2/DRS2MoleGame.h"
#include "Actor/DRCleanserSite.h"
#include "Actor/Stage2/DRS2Train.h"
#include "Actor/Stage2/DRS2TrainTrack.h"
#include "Actor/Stage2/DRS2TrainObstacle.h"
#include "Actor/Stage2/DRS2Barrier.h"
#include "DaeRune/DRLogChannels.h"

ADRS2StageDirector::ADRS2StageDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	// 서버 전용 레지스트리. 참조 대상들이 각자 복제되므로 이 액터는 복제할 필요가 없다.
	bReplicates = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
}

void ADRS2StageDirector::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority()) return;

	ValidateWiring();
}

void ADRS2StageDirector::ValidateWiring() const
{
	int32 MissingCount = 0;

	auto CheckRef = [&MissingCount](const UObject* Ref, const TCHAR* Label)
	{
		if (!IsValid(Ref))
		{
			UE_LOG(LogDR, Error, TEXT("[S2Director] 미배선: %s"), Label);
			++MissingCount;
		}
	};

	// 통로
	CheckRef(Blocker_StartToRoom1, TEXT("Blocker_StartToRoom1 (D0)"));
	CheckRef(Gate_Room1ToRoom2, TEXT("Gate_Room1ToRoom2 (D1)"));
	CheckRef(Gate_Room2Exit, TEXT("Gate_Room2Exit (E2)"));
	CheckRef(Blocker_Room1ToRoom3, TEXT("Blocker_Room1ToRoom3 (D2)"));
	CheckRef(Gate_Room3ToRoom4, TEXT("Gate_Room3ToRoom4 (D3)"));
	CheckRef(Gate_Room4Return, TEXT("Gate_Room4Return (R4)"));
	CheckRef(Blocker_Room3ToRoom5, TEXT("Blocker_Room3ToRoom5 (D4)"));
	CheckRef(Blocker_Room5ToRoom6, TEXT("Blocker_Room5ToRoom6 (D5)"));

	// 트리거
	CheckRef(Trigger_Room1, TEXT("Trigger_Room1"));
	CheckRef(Trigger_Room3, TEXT("Trigger_Room3"));
	CheckRef(Trigger_Room5, TEXT("Trigger_Room5"));

	// 방2 퍼즐
	CheckRef(Room2SlidePuzzle, TEXT("Room2SlidePuzzle (8퍼즐)"));
	CheckRef(Room2SwitchPuzzle, TEXT("Room2SwitchPuzzle (스위치)"));
	CheckRef(Room2CctvBoard, TEXT("Room2CctvBoard (CCTV)"));
	CheckRef(Room2Safe, TEXT("Room2Safe (금고)"));

	// 방3 / 방4
	CheckRef(Room4InstallSite, TEXT("Room4InstallSite (방4 설치대)"));
	CheckRef(Room4MoleGame, TEXT("Room4MoleGame (두더지 게임)"));
	CheckRef(Room4EntranceDropPoint, TEXT("Room4EntranceDropPoint (부품 복귀 지점)"));
	if (Room3RevivePoints.Num() == 0)
	{
		UE_LOG(LogDR, Error, TEXT("[S2Director] 미배선: Room3RevivePoints (부활 지점)"));
		++MissingCount;
	}

	// 방6 열차
	CheckRef(Train, TEXT("Train (열차)"));
	CheckRef(Track, TEXT("Track (선로)"));
	if (Obstacles.Num() != 3 || ForwardBarriers.Num() != 3 || RearBarriers.Num() != 3)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2Director] 방6 장애물/전방배리어/후방배리어는 각 3개여야 합니다 (현재 %d/%d/%d)."),
			Obstacles.Num(), ForwardBarriers.Num(), RearBarriers.Num());
		++MissingCount;
	}

	// 스폰 지점 (개수 경고)
	if (Room1SpawnPoints.Num() < 6)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[S2Director] Room1SpawnPoints %d개 - 4인 웨이브1(9마리) 동시 스폰에 부족할 수 있습니다."),
			Room1SpawnPoints.Num());
	}
	if (Room3SpawnPoints.Num() < 4)
	{
		UE_LOG(LogDR, Warning, TEXT("[S2Director] Room3SpawnPoints %d개 - 4개 이상 권장."), Room3SpawnPoints.Num());
	}
	if (Room5SpawnPoints.Num() < 4)
	{
		UE_LOG(LogDR, Warning, TEXT("[S2Director] Room5SpawnPoints %d개 - 4개 이상 권장."), Room5SpawnPoints.Num());
	}

	if (MissingCount > 0)
	{
		UE_LOG(LogDR, Error, TEXT("[S2Director] 배선 누락 %d건 - 스테이지2가 정상 진행되지 않습니다."), MissingCount);
	}
	else
	{
		UE_LOG(LogDR, Log, TEXT("[S2Director] 배선 검증 완료 (누락 없음)"));
	}
}
