// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Phase/Stage2/DRS2PhaseBase.h"
#include "DRS2DefensePhase.generated.h"

class ADRCharacter;
class ADRCleanserPart;
class ADRCleanserSite;
class ADRS2StageDirector;

/**
 * 스테이지2 페이즈 2 - 방3 무한 방어 + 방4 두더지 (Plan6 §14.3 · §14.4)
 *
 * 흐름:
 *   전원 방3 입장(부품 동반) -> D2 구조물 상승 봉쇄 + D3 게이트 발광(부품 소지자만)
 *   -> 부품 소지자 1명 방4 입장(게이트 즉시 잠김)
 *   -> 중앙 설치대에 부품 설치 = 두더지 게임 + 방3 50초 주기 무한 웨이브 동시 시작
 *   -> 두더지 20마리 클리어
 *      -> D3 재활성 + 사망자 방3 부활(체력 50%) + D4 하강 개방 + 스폰 중단 + 잔적 즉시 사망
 *      -> 완료
 *
 * 완료 조건은 "잔적 전멸"이 아니라 "두더지 클리어"다 (잔적은 즉시 사망 처리).
 *
 * 예외 처리 (§14.3.5):
 *   A) 방4 플레이어 사망   -> 두더지 리셋 + 부품을 설치대 옆에 드롭 + D3 를 Anyone 으로 재활성
 *                            + 방3 몬스터 전부 사망 + 스폰 중단 (재설치까지 웨이브 재개 없음)
 *   B) 방4 플레이어 이탈   -> 두더지 리셋 + 부품을 방3 쪽 문 앞으로 + D3 를 CarrierOnly 로 재활성
 *                            + 방3 몬스터 전부 소멸 + 스폰 중단 + 기준 인원 재계산
 */
UCLASS(Blueprintable)
class DAERUNE_API UDRS2DefensePhase : public UDRS2PhaseBase
{
	GENERATED_BODY()

public:
	virtual void OnPhaseStart() override;
	virtual void OnPhaseEnd() override;
	virtual bool IsCompleted() const override;

	virtual void NotifyPlayerDied(APlayerState* DeadPlayerState) override;
	virtual void NotifyPlayerLeft(APlayerState* LeftPlayerState) override;

protected:
	// ========== 설정 (BP) ==========

	// 인원별 웨이브 구성. 각 구간에 웨이브 구성 1개를 넣고 그것을 반복 스폰한다.
	// Plan6 §14.3.2 표: 1인 4마리 / 2인 5 / 3인 6 / 4인 8
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room3")
	TArray<FS2WaveSet> WaveSetsByPlayerCount;

	// 웨이브 주기 (초). Plan6 §14.3.2 확정값 50초
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room3", meta = (ClampMin = "1.0"))
	float WaveIntervalSeconds = 50.f;

	// 동시 생존 상한 (안전장치). 도달 시 해당 웨이브를 건너뛴다 (게임오버 아님).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room3", meta = (ClampMin = "1"))
	int32 MaxAliveEnemies = 30;

	// 부활 시 회복 비율 (최대 체력 기준). Plan6 §14.3.4 확정값 50%
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room3", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float ReviveHealthRatio = 0.5f;

	// 부활 시 물 회복 비율 (§14.3.6-6: 물 0으로 부활하면 즉시 부식 상태가 되므로 일정량 지급)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room3", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReviveWaterRatio = 0.5f;

	// 방3 스폰 지점의 RoomID
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room3")
	FName SpawnPointRoomID = TEXT("Room3");

	// 예외 처리에서 부품을 다시 만들 때 사용할 클래스 (금고에 넣은 것과 같은 BP)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room4")
	TSubclassOf<ADRCleanserPart> PartClass;

	// ========== 콜백 ==========

	UFUNCTION()
	void HandleAllInsideRoom3();

	UFUNCTION()
	void HandleInsideCountChanged(int32 InsideCount, int32 AliveTotal);

	UFUNCTION()
	void HandleRoom4Entered(ADRCharacter* Who);

	UFUNCTION()
	void HandlePartInstalled(ADRCleanserSite* Site);

	UFUNCTION()
	void HandleMoleProgress(int32 KillCount, int32 GoalKills);

	UFUNCTION()
	void HandleMoleGameCleared();

	// ========== 상태 ==========

	// 현재 방4에 들어가 있는 플레이어
	UPROPERTY()
	TWeakObjectPtr<ADRCharacter> Room4Player;

	UPROPERTY(BlueprintReadOnly, Category = "S2|Room3")
	bool bRoomStarted = false;

	UPROPERTY(BlueprintReadOnly, Category = "S2|Room3")
	bool bMoleGameCleared = false;

private:
	// 방3 웨이브 루프
	void StartWaveLoop();
	void StopWaveLoop();
	void SpawnOneWave();

	// 부품이 방3에 함께 들어왔는지 검증 (부품을 방1에 두고 오는 소프트락 방지)
	bool IsPartPresentInRoom3(ADRS2StageDirector* Director) const;

	// 월드에 부품이 하나도 없으면 안전망으로 하나 생성한다.
	// 정상 흐름에서는 방2를 완료해야(부품 소지자 퇴장) 방3에 오므로 부품이 반드시 존재한다.
	// 부품이 없다 = 페이즈 스킵 치트 등 비정상 진입이며, 그대로 두면 부품 동반 검증이
    // 영원히 실패해 방3이 시작되지 않는다.
	void EnsurePartExists(ADRS2StageDirector* Director);

	// 스폰된 적 전부 사망 처리 (사망 연출/물 보상 발생)
	void KillAllSpawnedEnemies();

	// 스폰된 적 전부 조용히 제거 (연출/보상 없음)
	void DestroyAllSpawnedEnemies();

	// 사망자 전원 부활
	void ReviveAllDeadPlayers(ADRS2StageDirector* Director);

	// 방4 재시도 준비 (예외 A/B 공통 처리)
	void PrepareRoom4Retry(ADRS2StageDirector* Director, bool bEjectPartInsideRoom4);

	FTimerHandle WaveLoopTimer;
};
