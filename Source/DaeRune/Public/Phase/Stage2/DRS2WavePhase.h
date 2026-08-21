// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Phase/Stage2/DRS2PhaseBase.h"
#include "DRS2WavePhase.generated.h"

/**
 * 스테이지2 페이즈 3 - 방5 3웨이브 (Plan6 §14.5)
 *
 * 흐름:
 *   전원 방5 입장 -> D4 구조물 재상승 봉쇄 -> 웨이브1 -> 웨이브2 -> 웨이브3 -> 완료
 *
 * ★웨이브 전환이 하이브리드다: min(30초 경과, 전원 전멸) 중 먼저 오는 쪽.
 *   - 방1: 시간 고정 (전멸해도 앞당기지 않음)
 *   - 방3: 시간 고정 무한 반복
 *   - 방5: 둘 중 먼저 오는 쪽  <- 이 페이즈만 다르다
 *
 * ★마지막 웨이브에는 다음 웨이브 타이머를 예약하지 않는다.
 *   예약하면 4번째 웨이브가 스폰되어 클리어가 불가능해진다.
 *
 * ★전멸 판정 기준은 웨이브 단위가 아니라 "전체 생존 수 0 && 대기 스폰 0"이다.
 *   30초 타이머로 웨이브가 겹쳐 스폰된 뒤라면 이전·현재 웨이브 적이 혼재하므로,
 *   혼재분까지 모두 전멸했을 때 다음 웨이브를 당긴다.
 */
UCLASS(Blueprintable)
class DAERUNE_API UDRS2WavePhase : public UDRS2PhaseBase
{
	GENERATED_BODY()

public:
	virtual void OnPhaseStart() override;
	virtual void OnPhaseEnd() override;
	virtual bool IsCompleted() const override;
	virtual void OnEnemyDeath(AActor* DeadEnemy) override;

protected:
	// ========== 설정 (BP) ==========

	// 인원별 웨이브 구성. [0]=1인 ... [3]=4인, 각 구간에 웨이브 3개.
	// Plan6 §14.5.2 표를 그대로 입력한다 (총 9 / 11 / 16 / 20마리).
	// ※ FS2WaveComposition::StartDelaySeconds 는 사용하지 않는다 - 전환 시점을 페이즈가 직접 제어한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room5")
	TArray<FS2WaveSet> WaveSetsByPlayerCount;

	// 웨이브 전환 간격 (초). Plan6 §14.5.3 확정값 30초.
	// 전원 전멸 시에는 이 타이머를 취소하고 즉시 다음 웨이브를 스폰한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room5", meta = (ClampMin = "1.0"))
	float WaveIntervalSeconds = 30.f;

	// 방5 스폰 지점의 RoomID
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room5")
	FName SpawnPointRoomID = TEXT("Room5");

	// ========== 콜백 ==========

	UFUNCTION()
	void HandleAllInsideRoom5();

	UFUNCTION()
	void HandleInsideCountChanged(int32 InsideCount, int32 AliveTotal);

	// ========== 런타임 상태 ==========

	UPROPERTY(BlueprintReadOnly, Category = "S2|Room5")
	int32 CurrentWaveIndex = INDEX_NONE;

	// 확정 인원 구간의 총 웨이브 수 (보통 3)
	UPROPERTY(BlueprintReadOnly, Category = "S2|Room5")
	int32 TotalWaveCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "S2|Room5")
	bool bWavesStarted = false;

private:
	// 웨이브 i 스폰 + (마지막이 아니면) 다음 웨이브 타이머 예약
	void StartWave(int32 WaveIndex);

	// 마지막 웨이브인지
	bool IsLastWave() const { return TotalWaveCount > 0 && CurrentWaveIndex >= TotalWaveCount - 1; }

	FTimerHandle NextWaveTimer;
};
