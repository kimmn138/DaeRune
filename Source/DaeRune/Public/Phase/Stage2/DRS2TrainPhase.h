// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Phase/Stage2/DRS2PhaseBase.h"
#include "DRS2TrainPhase.generated.h"

class ADRCharacter;
class ADREnemy;
class ADRS2MoleBoss;
class ADRS2StageDirector;

/**
 * 스테이지2 페이즈 4 - 방6 열차 탈출 (Plan6 §14.6)
 *
 * 흐름:
 *   D5 하강 개방 -> 4칸 열차에 상호작용 키로 탑승 -> 생존자 전원 착석 시 등속 출발
 *   -> 장애물 3구간: 정지 -> 두더지 보스가 장애물을 부수며 등장 -> 전투
 *      -> 체력 67% / 34% 도달 시 도망(비활성 보관) -> 전원 재탑승 -> 재출발
 *   -> 3구간에서 보스 처치 = 스테이지2 클리어
 *
 * ★보스는 단일 개체이며 체력이 구간을 넘어 이어진다(100% -> 67% -> 34% -> 처치).
 *   따라서 도망 시 Destroy 하지 않고 숨겨서 보관한다. 매 구간 새로 스폰하면
 *   어트리뷰트가 초기화되어 사양이 깨진다.
 *
 * ★3번째 구간에는 임계가 없다. 사망만이 해제 조건이다.
 *
 * 마지막 페이즈이므로 완료 시 GameMode 가 TriggerGameClear 를 자동 호출한다.
 * 원안의 "종점 도착 + 전원 도착지점 진입" 조건은 폐기되었다(§14.6.6).
 */
UCLASS(Blueprintable)
class DAERUNE_API UDRS2TrainPhase : public UDRS2PhaseBase
{
	GENERATED_BODY()

public:
	virtual void OnPhaseStart() override;
	virtual void OnPhaseEnd() override;
	virtual bool IsCompleted() const override;
	virtual void OnEnemyDeath(AActor* DeadEnemy) override;

protected:
	// ========== 설정 (BP) ==========

	// 두더지 보스. 스킬·패턴은 별도 작업이므로 임시로 EliteBear 계열을 지정해 루프를 검증한다(§14.6.7).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room6")
	TSubclassOf<ADREnemy> MoleBossClass;

	// 구간별 도망 임계 비율. Plan6 §14.6.4 확정값 {0.67, 0.34}.
	// 구간 수 = 임계 수 + 1 이며, 마지막 구간에는 임계가 없다(사망만이 조건).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room6")
	TArray<float> RetreatHealthRatios;

	// 등속 주행 속도 (uu/s)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Room6", meta = (ClampMin = "1.0"))
	float TrainSpeed = 600.f;

	// ========== 콜백 ==========

	UFUNCTION()
	void HandleBoardingChanged(int32 SeatedCount, int32 AliveTotal);

	UFUNCTION()
	void HandleTrainStopped(int32 ObstacleIndex);

	UFUNCTION()
	void HandleBossHealthChanged(float NewValue);

	UFUNCTION()
	void HandleBossMaxHealthChanged(float NewValue);

	// ========== 상태 ==========

	// 3구간에 반복 등장하는 단일 보스 (체력이 이어진다)
	UPROPERTY()
	TWeakObjectPtr<ADREnemy> MoleBoss;

	UPROPERTY(BlueprintReadOnly, Category = "S2|Room6")
	int32 CurrentObstacleIndex = INDEX_NONE;

	// 구간별 해제 래치 (임계·사망 이중 발화 방지)
	UPROPERTY()
	TArray<bool> bSegmentResolved;

	UPROPERTY(BlueprintReadOnly, Category = "S2|Room6")
	bool bBossDefeated = false;

	// 다음에 향할 장애물 인덱스 (구간 수를 넘으면 종점)
	UPROPERTY(BlueprintReadOnly, Category = "S2|Room6")
	int32 NextObstacleIndex = 0;

private:
	// 전원 착석 시 다음 목표로 출발
	void DepartToNextTarget(ADRS2StageDirector* Director);

	// 구간 해제 (보스 도망 + 전방 개방 + 재탑승 목표)
	void ResolveSegment(int32 ObstacleIndex);

	// 보스를 숨겨 보관한다 (체력 유지 목적, Destroy 금지)
	void StashBoss();

	// 보관 중인 보스를 다음 구간 위치에서 되살린다
	void ReappearBoss(const FTransform& SpawnTransform);

	// 체력 델리게이트 바인딩/해제
	void BindBossHealth();
	void UnbindBossHealth();

	// 보스 최대 체력 캐시 (비율 계산용)
	float CachedBossMaxHealth = 0.f;
};
