// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2MoleGame.generated.h"

class ADRS2Mole;

/** 난이도 티어 1구간 (Plan6 §14.4.3) */
USTRUCT(BlueprintType)
struct FS2MoleTier
{
	GENERATED_BODY()

	// 이 구간이 시작되는 누적 처치 수 (0 / 5 / 12)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 KillThreshold = 0;

	// 등장 유지 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float MoleLifetime = 2.0f;

	// 스폰 간격
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.05"))
	float SpawnInterval = 0.8f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMoleProgress, int32, KillCount, int32, GoalKills);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMoleGameCleared);

/**
 * 두더지 잡기 게임 관리 (Plan6 §14.4)
 *
 * 방4 중앙 설치대에 부품이 설치되면 시작된다. 20마리를 잡으면 클리어.
 * 잡은 마릿수에 따라 등장 유지 시간과 스폰 간격이 단계적으로 짧아진다.
 *
 * 스폰 간격 < 유지 시간이므로 두더지는 동시에 여러 마리 존재한다.
 * MaxConcurrentMoles 로 동시 존재 수를 제한한다.
 */
UCLASS()
class DAERUNE_API ADRS2MoleGame : public AActor
{
	GENERATED_BODY()

public:
	ADRS2MoleGame();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 게임 시작 (서버). 처치 수를 0으로 초기화하고 스폰을 시작한다.
	UFUNCTION(BlueprintCallable, Category = "S2|MoleGame")
	void StartGame();

	// 중단 + 초기화 (서버). 방4 플레이어 사망/이탈 시 사용한다.
	// 두더지를 전부 제거하고 처치 수를 0으로 되돌린다.
	UFUNCTION(BlueprintCallable, Category = "S2|MoleGame")
	void AbortAndReset();

	// 두더지가 알리는 결과 (서버)
	void OnMoleKilled(ADRS2Mole* Mole);
	void OnMoleExpired(ADRS2Mole* Mole);

	UFUNCTION(BlueprintCallable, Category = "S2|MoleGame")
	int32 GetKillCount() const { return KillCount; }

	UFUNCTION(BlueprintCallable, Category = "S2|MoleGame")
	int32 GetGoalKills() const { return GoalKills; }

	// 서버 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "S2|MoleGame")
	FOnMoleProgress OnProgress;

	UPROPERTY(BlueprintAssignable, Category = "S2|MoleGame")
	FOnMoleGameCleared OnCleared;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ========== 설정 ==========

	// 난이도 티어. Plan6 §14.4.3 표: {0, 2.0, 0.8} / {5, 1.3, 0.5} / {12, 0.8, 0.3}
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|MoleGame")
	TArray<FS2MoleTier> Tiers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|MoleGame", meta = (ClampMin = "1"))
	int32 GoalKills = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|MoleGame", meta = (ClampMin = "1"))
	int32 MaxConcurrentMoles = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|MoleGame")
	TSubclassOf<ADRS2Mole> MoleClass;

	// 바닥 등장 지점. 레벨에 배치한 액터를 배선한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|MoleGame")
	TArray<TObjectPtr<AActor>> SpawnPoints;

	// ========== 복제 상태 ==========

	// 진행도 UI 용
	UPROPERTY(ReplicatedUsing = OnRep_KillCount, BlueprintReadOnly, Category = "S2|MoleGame")
	int32 KillCount = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "S2|MoleGame")
	bool bActive = false;

	UFUNCTION()
	void OnRep_KillCount();

	// 진행도 표시 연출 훅
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|MoleGame")
	void OnKillCountChanged(int32 NewKillCount, int32 InGoalKills);

private:
	// 현재 티어 (누적 처치 수 기준, KillThreshold 역순 탐색)
	const FS2MoleTier& GetCurrentTier() const;

	// 스폰 타이머를 현재 티어 간격으로 (재)설정
	void RestartSpawnTimer();

	void StopSpawning();

	// 두더지 1기 스폰
	void SpawnMole();

	// 살아있는 두더지 전부 제거 (연출 없이)
	void DestroyAllMoles();

	UPROPERTY()
	TArray<TWeakObjectPtr<ADRS2Mole>> ActiveMoles;

	FTimerHandle SpawnTimer;

	// 직전 스폰 지점 (연속 중복 완화)
	int32 LastSpawnPointIndex = INDEX_NONE;

	// Tiers 가 비어 있을 때 사용할 기본값
	static const FS2MoleTier DefaultTier;
};
