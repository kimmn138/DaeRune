// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Phase/Stage2/DRS2PhaseBase.h"
#include "DRS2CombatPhase.generated.h"

/**
 * 스테이지2 페이즈 0 - 방1 전투 (Plan6 §14.1)
 *
 * 흐름:
 *   전원 방1 입장 -> D0 구조물 상승 봉쇄 -> 웨이브1 즉시 -> 30초 후 웨이브2 -> 전멸 -> 완료
 *
 * 웨이브 진행은 시간 기반이다. 웨이브1을 30초 안에 전멸시켜도 웨이브2가 예약되어 있어
 * PendingSpawnCount 가 0이 아니므로 페이즈가 조기 종료되지 않는다.
 *
 * 적은 레벨에 배치한 ADRS2EnemySpawnPoint(RoomID = SpawnPointRoomID) 위치에서 스폰된다.
 */
UCLASS(Blueprintable)
class DAERUNE_API UDRS2CombatPhase : public UDRS2PhaseBase
{
	GENERATED_BODY()

public:
	virtual void OnPhaseStart() override;
	virtual void OnPhaseEnd() override;
	virtual bool IsCompleted() const override;
	virtual void OnEnemyDeath(AActor* DeadEnemy) override;

protected:
	// ========== 설정 (BP) ==========

	// 인원별 웨이브 구성. [0]=1인 ... [3]=4인, 각 구간에 웨이브 2개 (StartDelay 0초 / 30초).
	// Plan6 §14.1.3 표를 그대로 입력한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room1")
	TArray<FS2WaveSet> WaveSetsByPlayerCount;

	// 이 페이즈가 사용할 스폰 지점의 RoomID
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room1")
	FName SpawnPointRoomID = TEXT("Room1");

	// ========== 콜백 ==========

	UFUNCTION()
	void HandleAllInsideRoom1();

	UFUNCTION()
	void HandleInsideCountChanged(int32 InsideCount, int32 AliveTotal);

	// ========== 런타임 상태 ==========

	// 확정 인원의 전 웨이브 마리 수 합계 (목표 진행도 분모)
	UPROPERTY(BlueprintReadOnly, Category = "S2|Room1")
	int32 TotalSpawnCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "S2|Room1")
	int32 KilledCount = 0;

	// 전원 입장 후 전투가 시작되었는지 (입장 전 전멸 판정이 성립하는 것을 막는 래치)
	UPROPERTY(BlueprintReadOnly, Category = "S2|Room1")
	bool bCombatStarted = false;
};
