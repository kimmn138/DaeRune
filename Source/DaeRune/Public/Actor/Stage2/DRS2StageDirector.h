// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2StageDirector.generated.h"

class ADRS2PassageBlocker;
class ADRS2TeleportGate;
class ADRS2RoomTrigger;
class ADRS2SlidePuzzle;
class ADRS2SwitchPuzzle;
class ADRS2CctvBoard;
class ADRS2Safe;
class ADRS2MoleGame;
class ADRCleanserSite;
class ADRS2Train;
class ADRS2TrainTrack;
class ADRS2TrainObstacle;
class ADRS2Barrier;

/**
 * 스테이지2 배선 레지스트리 (Plan6 §4.3)
 *
 * 페이즈는 GameMode가 NewObject로 만드는 UObject라 레벨 액터를 EditInstanceOnly로
 * 직접 참조할 수 없다. 그래서 레벨에 1개 배치하는 중앙 레지스트리를 둔다.
 *
 * 서버 전용 데이터라 복제하지 않는다 (참조 대상인 문/트리거 등이 각자 복제된다).
 * BeginPlay에서 미배선 참조를 전부 Error 로그로 나열해 레벨 배선 실수를 조기에 잡는다.
 *
 * 방2/방3/방4/방6 전용 참조는 해당 마일스톤에서 추가한다 (M3~M6).
 */
UCLASS()
class DAERUNE_API ADRS2StageDirector : public AActor
{
	GENERATED_BODY()

public:
	ADRS2StageDirector();

	// ========== 통로 8곳 (Plan6 §1.1) ==========

	// D0: 시작지점 -> 방1 (상승 구조물)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|통로")
	TObjectPtr<ADRS2PassageBlocker> Blocker_StartToRoom1;

	// D1: 방1 -> 방2 (Individual 게이트)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|통로")
	TObjectPtr<ADRS2TeleportGate> Gate_Room1ToRoom2;

	// E2: 방2 출구 -> 방1 (TeamOnCarrier 게이트, 상시 활성)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|통로")
	TObjectPtr<ADRS2TeleportGate> Gate_Room2Exit;

	// D2: 방1 <-> 방3 (개찰구 회전문)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|통로")
	TObjectPtr<ADRS2PassageBlocker> Blocker_Room1ToRoom3;

	// D3: 방3 -> 방4 (CarrierOnly + 통과 시 자동 비활성)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|통로")
	TObjectPtr<ADRS2TeleportGate> Gate_Room3ToRoom4;

	// R4: 방4 -> 방3 복귀 (두더지 클리어 시 활성)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|통로")
	TObjectPtr<ADRS2TeleportGate> Gate_Room4Return;

	// D4: 방3 <-> 방5 (개찰구 회전문, 닫힘 시작 -> 열림 -> 재차 닫힘 왕복)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|통로")
	TObjectPtr<ADRS2PassageBlocker> Blocker_Room3ToRoom5;

	// D5: 방5 -> 방6 (하강 개방 구조물)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|통로")
	TObjectPtr<ADRS2PassageBlocker> Blocker_Room5ToRoom6;

	// ========== 방 입장 트리거 ==========

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|트리거")
	TObjectPtr<ADRS2RoomTrigger> Trigger_Room1;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|트리거")
	TObjectPtr<ADRS2RoomTrigger> Trigger_Room3;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|트리거")
	TObjectPtr<ADRS2RoomTrigger> Trigger_Room5;

	// ========== 적 스폰 지점 ==========

	// 방1: 4인 웨이브1이 9마리 동시 스폰이므로 6개 이상 권장
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|스폰")
	TArray<TObjectPtr<AActor>> Room1SpawnPoints;

	// 방3: 4인 웨이브가 8마리
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|스폰")
	TArray<TObjectPtr<AActor>> Room3SpawnPoints;

	// 방5: 4인 웨이브3이 7마리 (잠자리 비중이 높아 공중 지점 별도 권장)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|스폰")
	TArray<TObjectPtr<AActor>> Room5SpawnPoints;

	// ========== 방2 퍼즐 (Plan6 §14.2) ==========

	// 8퍼즐 (UI 방식). 금고 비밀번호 첫 자리를 공개한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|방2")
	TObjectPtr<ADRS2SlidePuzzle> Room2SlidePuzzle;

	// 스위치 퍼즐. 3라운드 클리어 시 둘째 자리를 공개한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|방2")
	TObjectPtr<ADRS2SwitchPuzzle> Room2SwitchPuzzle;

	// CCTV. 해결 상태가 없고 관찰 결과가 마지막 자리가 된다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|방2")
	TObjectPtr<ADRS2CctvBoard> Room2CctvBoard;

	// 금고. 개방 시 내부에서 부품이 스폰된다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|방2")
	TObjectPtr<ADRS2Safe> Room2Safe;

	// ========== 방3 / 방4 (Plan6 §14.3 · §14.4) ==========

	// 두더지 클리어 시 사망자를 되살릴 지점 (인원 수만큼 배치 권장)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|방3")
	TArray<TObjectPtr<AActor>> Room3RevivePoints;

	// 방4 플레이어 접속 종료 시 부품을 놓을 지점 (방3 쪽 D3 게이트 앞)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|방3")
	TObjectPtr<AActor> Room4EntranceDropPoint;

	// 방4 중앙 설치대 ("CleanserSite" 태그 필요)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|방4")
	TObjectPtr<ADRCleanserSite> Room4InstallSite;

	// 두더지 게임 관리 액터
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|방4")
	TObjectPtr<ADRS2MoleGame> Room4MoleGame;

	// ========== 방6 열차 (Plan6 §14.6) ==========

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|방6")
	TObjectPtr<ADRS2Train> Train;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|방6")
	TObjectPtr<ADRS2TrainTrack> Track;

	// 순서 = 진행 순서 (3개). 각자 BossSpawnPoint 를 보유한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|방6")
	TArray<TObjectPtr<ADRS2TrainObstacle>> Obstacles;

	// 전방 차단 (Obstacles 와 동일 인덱스). 장애물이 부서지므로 필요하다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|방6")
	TArray<TObjectPtr<ADRS2Barrier>> ForwardBarriers;

	// 후방 차단 (Obstacles 와 동일 인덱스)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|방6")
	TArray<TObjectPtr<ADRS2Barrier>> RearBarriers;

protected:
	virtual void BeginPlay() override;

private:
	// 배선 누락을 전부 나열한다 (서버 전용)
	void ValidateWiring() const;
};
