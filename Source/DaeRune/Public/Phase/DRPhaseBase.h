// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DRPhaseBase.generated.h"

class ADRStageGameMode;
class ADRStageGameState;
class ADRCleanserSite;
class ADREnemy;

USTRUCT(BlueprintType)
struct FPhaseObjectiveData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PhaseNumber = 1;

	// 목표 문구 (예: "클렌저를 확보하세요")
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText ObjectiveTitle;

	// 진행도 단위 (예: "확보한 클렌저")
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText ProgressFormat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequiredCount = 0;

	// 페이즈 진입 배너 문구 (Plan6 §5.3)
	// 비어 있으면 배너를 띄우지 않는다 - 한 페이즈 안에서 목표만 교체하는 서브 목표 전환용.
	// 비어 있는 경우 OverlayWidgetController가 레거시 switch 문구로 폴백한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText PhaseAlarmText;
};

/**
 * 모든 지점을 한 번씩 쓴 뒤 다시 채우는 비반복 랜덤 인덱스 풀.
 * 적 스폰 지점을 고르는 페이즈들이 같은 규칙을 쓴다.
 */
struct FDRShuffledIndexPool
{
	// 0 .. InNum-1 로 풀을 다시 만든다
	void Reset(int32 InNum);

	// 남은 인덱스 하나를 무작위로 꺼낸다. 풀이 비면 전체로 다시 채운다. 크기가 0 이면 INDEX_NONE.
	int32 Pick();

private:
	int32 Num = 0;
	TArray<int32> Remaining;
};

/**
 * 모든 페이즈의 기본 클래스.
 * GameMode 가 만들고 OnPhaseStart / OnPhaseEnd 로 수명을 관리한다.
 */
UCLASS(Blueprintable, Abstract)
class DAERUNE_API UDRPhaseBase : public UObject
{
	GENERATED_BODY()

public:
	// ========== 수명 ==========

	// GameMode 가 생성 직후 호출
	UFUNCTION(BlueprintCallable, Category = "Phase")
	virtual void Initialize(ADRStageGameMode* InGameMode, ADRStageGameState* InGameState);

	// 페이즈 시작 (GameMode 가 호출)
	UFUNCTION(BlueprintCallable, Category = "Phase")
	virtual void OnPhaseStart();

	// 페이즈 종료 (정리 작업). GameMode 는 진행 중인 페이즈에만 한 번 호출한다.
	UFUNCTION(BlueprintCallable, Category = "Phase")
	virtual void OnPhaseEnd();

	// 이 페이즈의 완료 조건 충족 여부 (파생 클래스가 구현)
	// GameMode의 인덱스 switch 대신 각 페이즈가 스스로 완료를 판정한다
	virtual bool IsCompleted() const { return false; }

	// OnPhaseStart 이후 OnPhaseEnd 전까지 true
	bool IsPhaseActive() const { return bIsPhaseActive; }

	// ========== 클렌저 사이트 ==========

	// 맵의 전체 클렌저 사이트 설정 (GameMode가 초기화 시 호출)
	UFUNCTION(BlueprintCallable, Category = "Phase")
	void SetCleanserSites(const TArray<ADRCleanserSite*>& InCleanserSites);

	const TArray<TObjectPtr<ADRCleanserSite>>& GetCleanserSites() const { return CleanserSites; }

	// 앞 페이즈에서 고른 활성 사이트를 다음 페이즈로 넘길 때 사용
	void SetActiveCleanserSites(const TArray<TObjectPtr<ADRCleanserSite>>& InActiveSites);
	const TArray<TObjectPtr<ADRCleanserSite>>& GetActiveCleanserSites() const { return ActiveCleanserSites; }

	// ========== 적 추적 ==========

	// 추적 중인 적이 죽었을 때 (사망 델리게이트)
	UFUNCTION()
	virtual void OnEnemyDeath(AActor* DeadEnemy);

	// ========== 플레이어 상태 통지 (Plan6 §14.3.5) ==========
	// GameMode가 사망/접속 종료를 현재 페이즈에 전달한다.
	// 스테이지2 방3+4처럼 특정 플레이어의 이탈이 진행 상태를 되돌려야 하는 페이즈가 사용한다.

	// 플레이어 사망 (아직 게임에는 남아 있음)
	virtual void NotifyPlayerDied(APlayerState* DeadPlayerState) {}

	// 플레이어 접속 종료 (영구 이탈)
	virtual void NotifyPlayerLeft(APlayerState* LeftPlayerState) {}

protected:
	// ========== 적 스폰 / 추적 헬퍼 ==========

	/**
	 * 적을 지연 스폰한다. Configure 는 BeginPlay 전에 호출된다 (레벨 · 페이즈 플래그 주입용).
	 * 추적 목록에는 넣지 않는다 — 일반 적은 TrackSpawnedEnemy 로 따로 등록한다.
	 */
	ADREnemy* SpawnEnemyDeferred(TSubclassOf<ADREnemy> EnemyClass, const FTransform& SpawnTransform,
		TFunctionRef<void(ADREnemy&)> Configure);
	ADREnemy* SpawnEnemyDeferred(TSubclassOf<ADREnemy> EnemyClass, const FTransform& SpawnTransform);

	/**
	 * 적을 추적 목록에 넣고 사망 · 소멸 델리게이트를 묶는다.
	 * ★소멸(OnDestroyed)도 본다★ — 맵 밖으로 떨어져 KillZ 로 사라지는 적은 사망 신호 없이 Destroy 되므로,
	 * 그대로 두면 영원히 "살아 있는 적"으로 남아 웨이브·방이 끝나지 않는다.
	 */
	void TrackSpawnedEnemy(AActor* Enemy);

	// 추적 중인 적을 조용히 제거한다 (연출 · 보상 없음, 사망 처리로 세지 않는다)
	void DestroyTrackedEnemies();

	// 살아 있는(파괴되지 않은) 추적 중인 적 수
	UFUNCTION(BlueprintCallable, Category = "Phase")
	int32 GetAliveEnemyCount() const;

	// ========== 상태 ==========

	// 페이즈가 진행 중인지
	UPROPERTY(BlueprintReadOnly, Category = "Phase")
	bool bIsPhaseActive = false;

	// 이 페이즈가 스폰해 추적 중인 적
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SpawnedEnemies;

	UPROPERTY()
	TObjectPtr<ADRStageGameMode> GameMode;

	UPROPERTY()
	TObjectPtr<ADRStageGameState> GameState;

	// 맵에 배치된 전체 클렌저 사이트
	UPROPERTY(BlueprintReadOnly, Category = "Phase")
	TArray<TObjectPtr<ADRCleanserSite>> CleanserSites;

	// 이 페이즈가 쓰는 활성 클렌저 사이트 (앞 페이즈에서 넘어온다)
	UPROPERTY(BlueprintReadOnly, Category = "Phase")
	TArray<TObjectPtr<ADRCleanserSite>> ActiveCleanserSites;

	// ========== 페이즈 목표 UI ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Config")
	TObjectPtr<UDataTable> PhaseObjectiveDataTable;

	// "Phase%d" 행으로 목표를 설정한다
	void SetupPhaseObjective(int32 PhaseNumber);

	/**
	 * 행 이름을 직접 지정해 목표를 설정한다 (Plan6 §5.2).
	 * 한 페이즈 안에서 목표를 여러 번 교체하는 스테이지2 페이즈들이 사용한다.
	 * OverrideRequiredCount 가 0 이상이면 진행도 분모를 런타임 값으로 덮어쓴다
	 * (인원별 스폰 수, 두더지 목표 수처럼 실행 중에 분모가 정해지는 목표).
	 */
	void SetupPhaseObjectiveByRow(FName RowName, int32 OverrideRequiredCount = INDEX_NONE);

private:
	// 추적 중인 적이 사망 신호 없이 사라졌을 때 (KillZ 낙하 등)
	UFUNCTION()
	void HandleTrackedEnemyDestroyed(AActor* DestroyedActor);

	// 사망 · 소멸 델리게이트 해제
	void UnbindTrackedEnemy(AActor* Enemy);
};
