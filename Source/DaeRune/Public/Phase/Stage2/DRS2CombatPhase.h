// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Phase/Stage2/DRS2PhaseBase.h"
#include "DRS2CombatPhase.generated.h"

/**
 * 스테이지2 페이즈 0 - 방1 전투 (Plan6 §14.1)
 *
 * 흐름:
 *   전원 방1 입장 -> D0 구조물 상승 봉쇄 -> 웨이브1 즉시 -> 웨이브2 -> 전멸 -> 완료
 *
 * 다음 웨이브는 예약 시각(StartDelaySeconds, 30초)과 "나온 적 전멸" 중 먼저 오는 쪽에 나온다.
 * 웨이브1을 30초 안에 다 잡으면 기다리지 않고 바로 웨이브2가 나온다. 30초가 먼저 오면 웨이브1이 남아 있어도 나온다.
 * 남은 웨이브는 PendingSpawnCount 에 들어 있으므로 웨이브1 전멸이 방 완료로 오인되지 않는다.
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

	// ========== 방 트리거 ==========

	// 전원 방1 입장 → 봉쇄 + 웨이브 예약
	virtual void OnRoomAllInside() override;
	virtual bool HasRoomStarted() const override { return bCombatStarted; }

	// ========== 런타임 상태 ==========

	// 확정 인원의 전 웨이브 마리 수 합계 (목표 진행도 분모)
	UPROPERTY(BlueprintReadOnly, Category = "S2|Room1")
	int32 TotalSpawnCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "S2|Room1")
	int32 KilledCount = 0;

	// 전원 입장 후 전투가 시작되었는지 (입장 전 전멸 판정이 성립하는 것을 막는 래치)
	UPROPERTY(BlueprintReadOnly, Category = "S2|Room1")
	bool bCombatStarted = false;

private:
	// 웨이브 하나를 내보낸다 (예약 타이머 또는 앞 웨이브 전멸). 이미 나온 웨이브면 무시한다
	void StartWave(int32 WaveIndex);

	// 아직 안 나온 웨이브 중 가장 앞 번호 (없으면 INDEX_NONE)
	int32 FindNextUnstartedWave() const;

	// 웨이브별 예약 타이머 · 마리 수 · 나왔는지 (인덱스 = 웨이브 세트 순서)
	TArray<FTimerHandle> WaveTimers;
	TArray<int32> WaveSpawnCounts;
	TArray<bool> WaveStarted;

	// 아직 안 나온 웨이브들의 마리 수 합. PendingSpawnCount 가 이보다 크면 나오는 중인 적이 있다
	int32 UnstartedSpawnCount = 0;
};
