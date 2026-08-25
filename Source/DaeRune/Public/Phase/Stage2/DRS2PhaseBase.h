// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Phase/DRPhaseBase.h"
#include "Phase/Stage2/DRS2Types.h"
#include "DRS2PhaseBase.generated.h"

class ADRS2StageDirector;
class ADRS2PassageBlocker;
class ADRS2TeleportGate;
class ADREnemy;

/**
 * 스테이지2 페이즈 공통 베이스 (Plan6 §4.4)
 *
 * 5개 페이즈가 공유하는 기능:
 *  - Director 접근 (레벨 액터 배선 레지스트리)
 *  - 기준 인원 확정 및 인원별 웨이브 세트 조회
 *  - 웨이브 스폰 (마리 단위 셔플 + 지연 스폰 + 대기 수 추적)
 *  - 통로 제어 래퍼 (널 가드)
 *  - 타이머 일괄 정리
 *
 * 전멸 판정 공통 규칙:
 *   GetAliveEnemyCount() == 0 && PendingSpawnCount == 0
 * PendingSpawnCount 는 아직 스폰되지 않은(예약된) 마리 수다. 이 규칙 덕에
 * 방1에서 웨이브1을 30초 안에 전멸시켜도 페이즈가 조기 종료되지 않는다.
 */
UCLASS(Abstract, Blueprintable)
class DAERUNE_API UDRS2PhaseBase : public UDRPhaseBase
{
	GENERATED_BODY()

public:
	virtual void OnPhaseEnd() override;

protected:
	// ========== Director ==========

	// TActorIterator로 1회 탐색 후 캐시. 없으면 Error 로그 1회.
	ADRS2StageDirector* GetDirector();

	// ========== 인원 / 웨이브 ==========

	// 방 시작 시 1회 호출. 생존자 수를 1~4로 clamp해 BasePlayerCount에 확정한다.
	// 이후 사망해도 재계산하지 않는다 (죽으면 쉬워지는 역인센티브 방지 + UI 분모 일관성).
	int32 ResolveBasePlayerCount();

	// BasePlayerCount에 해당하는 웨이브 세트 조회. 구간이 부족하면 마지막 구간으로 폴백.
	const FS2WaveSet* ResolveWaveSet(const TArray<FS2WaveSet>& WaveSets) const;

	// 웨이브 세트 전체의 스폰 마리 수 합계 (목표 UI 분모 + PendingSpawnCount 초기값)
	static int32 CountTotalSpawns(const FS2WaveSet& WaveSet);

	// 웨이브 1개 구성의 마리 수
	static int32 CountComposition(const FS2WaveComposition& Composition);

	// ========== 스폰 지점 ==========

	// 레벨에 배치된 ADRS2EnemySpawnPoint 중 RoomID가 일치하는 것을 수집한다 (스테이지1 관례 계승).
	// DirectorOverride 가 비어있지 않으면 그쪽을 우선 사용한다 (명시 배선이 필요한 경우).
	// 방 전투 시작 시 1회 호출한다.
	void InitSpawnPoints(FName RoomID, const TArray<TObjectPtr<AActor>>& DirectorOverride);

	// 비반복 랜덤 선택: 모든 지점을 한 번씩 사용한 뒤 풀을 다시 채운다 (UDRPhase3::SelectNextSpawnPoint 관례).
	AActor* SelectNextSpawnPoint();

	int32 GetSpawnPointCount() const { return SpawnPoints.Num(); }

	// ========== 웨이브 스폰 ==========

	// 세트 안의 모든 웨이브를 StartDelaySeconds에 맞춰 예약한다. (InitSpawnPoints 선행 필요)
	void ScheduleWaveSet(const FS2WaveSet& WaveSet);

	// 웨이브 1개를 스폰한다 (마리 단위 셔플 후 PerEnemySpawnInterval 간격으로 순차 스폰).
	void SpawnComposition(FS2WaveComposition Composition);

	// 스폰 + 사망 델리게이트 바인딩 + SpawnedEnemies 추적
	/**
	 * 스폰 + 사망 델리게이트 바인딩 + SpawnedEnemies 추적
	 *
	 * @param EnemyLevel  INDEX_NONE 이면 BP 기본 레벨을 그대로 쓴다(기존 동작).
	 *                    1 이상이면 ★BeginPlay 전에 SetLevel 을 넣기 위해 지연 스폰 경로를 탄다.
	 *                    ADREnemy::BeginPlay 안에서 InitializeDefaultAttributes(Level) 와
	 *                    GiveStartupAbilities(Level) 가 모두 끝나므로, 스폰이 끝난 뒤에
	 *                    SetLevel 을 불러도 어트리뷰트와 어빌리티 스펙 레벨에는 반영되지 않는다.
	 */
	AActor* SpawnEnemyAt(TSubclassOf<ADREnemy> EnemyClass, const FTransform& SpawnTransform, int32 EnemyLevel = INDEX_NONE);

	// ========== 통로 제어 (널 가드 포함) ==========

	void SetBlockerBlocked(ADRS2PassageBlocker* Blocker, bool bBlocked);
	void SetGateActive(ADRS2TeleportGate* Gate, bool bActive);

	// ========== 타이머 ==========

	// 관리 배열에 새 핸들 슬롯을 추가하고 참조를 돌려준다.
	FTimerHandle& AddManagedTimer();

	// 이 페이즈가 예약한 모든 타이머 해제
	void ClearAllManagedTimers();

	// GameMode의 World 접근 헬퍼 (페이즈는 UObject라 GetWorld가 없다)
	UWorld* GetPhaseWorld() const;

	// ========== 상태 ==========

	// 방 시작 시점 생존자 수 (확정 후 불변)
	UPROPERTY(BlueprintReadOnly, Category = "S2|Phase")
	int32 BasePlayerCount = 0;

	// 아직 스폰되지 않은 마리 수 (예약된 다음 웨이브 포함)
	UPROPERTY(BlueprintReadOnly, Category = "S2|Phase")
	int32 PendingSpawnCount = 0;

	UPROPERTY()
	TObjectPtr<ADRS2StageDirector> CachedDirector;

private:
	// 마리 1기 스폰 (지연 스폰 타이머 콜백)
	void SpawnOneEnemy(TSubclassOf<ADREnemy> EnemyClass);

	TArray<FTimerHandle> ManagedTimers;

	// 현재 방의 스폰 지점 (InitSpawnPoints 로 채운다)
	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnPoints;

	// 아직 사용하지 않은 스폰 지점 인덱스 풀 (비면 전체로 다시 채운다)
	TArray<int32> RemainingSpawnPointIndices;

	// Director 탐색 실패 로그를 1회만 남기기 위한 플래그
	bool bDirectorLookupFailed = false;
};
