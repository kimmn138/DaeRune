// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Phase/DRPhaseBase.h"
#include "Phase/DRPhase3DataTypes.h"
#include "DRPhase3.generated.h"

class ADRPoisonGasActor;
class ADREnemy;
class ADRCharacter;
class ADRCleanserSite;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EPoisonGasSpawnPointType : uint8
{
	Normal UMETA(DisplayName = "Normal (Green)"),
	CleanserLinked UMETA(DisplayName = "Cleanser Linked (Blue)")
};

USTRUCT(BlueprintType)
struct FPoisonGasSpawnPointData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPoisonGasSpawnPointType SpawnType = EPoisonGasSpawnPointType::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "SpawnType == EPoisonGasSpawnPointType::CleanserLinked"))
	FName LinkedCleanserTag = NAME_None;
};

UENUM(BlueprintType)
enum class EWaveState : uint8
{
	Waiting UMETA(DisplayName = "Waiting"),
	InProgress UMETA(DisplayName = "In Progress"),
	Rest UMETA(DisplayName = "Rest"),
	Completed UMETA(DisplayName = "Completed")
};

USTRUCT(BlueprintType)
struct FWaveData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PlayDuration = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float RestDuration = 10.0f;
};

USTRUCT(BlueprintType)
struct FWaveLevelModifier
{
	GENERATED_BODY()

	// 스폰 배치(4마리 동시 스폰) 간격
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float SpawnIntervalSeconds = 2.5f;

	// 플레이어 1인당 사이클 길이(4의 배수)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "4"))
	int32 SpawnCycleLengthPerPlayer = 8;

	// 몬스터 스폰 사이클: 1=일반, 2=돌진, 3=은신
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "3"))
	TArray<int32> MonsterSpawnCycle = { 1, 1, 1, 1, 1, 1, 1, 1 };

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bSpawnToxicGas = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bSpawnEliteBoss = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCleanserSiteReadySignature, ADRCleanserSite*, FirstCleanserSite, ADRCleanserSite*, SecondCleanserSite);

UCLASS()
class DAERUNE_API UDRPhase3 : public UDRPhaseBase
{
	GENERATED_BODY()

public:
	UDRPhase3();

	virtual void OnPhaseStart() override;
	virtual void OnPhaseEnd() override;
	virtual void OnEnemyDeath(AActor* DeadEnemy) override;

	// 치트: 현재 웨이브에서 스폰된 적을 모두 제거하고 다음 웨이브로 즉시 진행. 마지막 웨이브였다면 페이즈 종료.
	void SkipToNextWave();

	UFUNCTION()
	void OnEliteEnemyDeath(AActor* DeadEnemy);

	UPROPERTY(BlueprintAssignable)
	FOnCleanserSiteReadySignature OnCleanserSiteReadyDelegate;

protected:
	virtual void BeginDestroy() override;

	UFUNCTION()
	void StartNextWave();

	UFUNCTION()
	void EndCurrentWave();

	UFUNCTION()
	void StartRestTime();

	UFUNCTION()
	void EndRestTime();

	UFUNCTION()
	void ProcessWaveSpawn();

	void CheckWaveCompletion();

	// 적 스폰 시스템
	void FindEnemySpawnPoints();
	AActor* SelectNextSpawnPoint();
	TSubclassOf<ADREnemy> SelectMonsterClassByType(int32 MonsterType) const;
	int32 GetCurrentCycleMonsterType(const FWaveLevelModifier& WaveModifier) const;
	void SpawnMonsterByCycle();

	void SpawnEliteMonster(const FVector& SpawnLocation);

	FWaveLevelModifier GetWaveLevelModifier(int32 WaveLevel) const;
	FWaveLevelModifier GetDefaultWaveLevelModifier(int32 WaveLevel) const;
	FWaveData GetWaveData(int32 WaveNumber) const;
	FWaveData GetDefaultWaveData(int32 WaveNumber) const;

	void LoadPhase3ConfigFromBalanceConfig();

	void InitializeCleanserSite();
	void InitializeActiveSpawnPoints();

	UFUNCTION()
	void OnCleanserSiteDestroyed(ADRCleanserSite* DestroyedSite) const;

	UFUNCTION()
	void OnCleanserSiteHealthBelowHalf();

	UFUNCTION()
	void OnCleanserSiteHealthZero();

	void IncreaseWaveLevel(int32 Amount = 1);

	void CheckGameOverConditions() const;
	void CheckVictoryConditions();
	bool IsMonsterCountExceeded() const;

	TArray<int32> SelectRandomSpawnPointIndices() const;
	void SpawnToxicGas();
	void SpawnPoisonGasActor();
	void RemoveToxicGas();

	// 독가스 스폰 위치 마커 액터의 태그
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase3|PoisonGas")
	FName PoisonGasSpawnPointTag = "PoisonGasSpawnPoint";

	// CleanserLinked 판별용 태그 접두사 (예: "PoisonGas_" + CleanserID)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase3|PoisonGas")
	FName PoisonGasCleanserTagPrefix = "PoisonGas_";

	TArray<FPoisonGasSpawnPointData> AllPoisonGasSpawnPoints;

	float PoisonGasSpawnInterval = 10.0f;

	// 엘리트 보스 스폰 위치를 찾을 때 사용할 태그
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase3|Config")
	FName BossSpawnPointTag = "Phase3SpawnPoint";

	// 고정 적 스폰 포인트(스1~스4)를 찾을 태그
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase3|Spawn")
	FName EnemySpawnPointTag = "Phase3EnemySpawnPoint";

	void FindPoisonGasSpawnPoints();

private:
	void GrantEliteBossTag();
	void RemoveEliteBossTag();

	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Config|DataTable")
	TObjectPtr<UDataTable> WaveDataTable;

	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Config|DataTable")
	TObjectPtr<UDataTable> WaveLevelModifierTable;

	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Config|Fallback")
	TArray<FWaveData> WaveDataArray;

	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Config|Fallback")
	TMap<int32, FWaveLevelModifier> WaveLevelModifiers;

	int32 MaxMonsterCount = 100;
	float DefenseDuration = 300.0f;
	float SpawnDistanceMin = 500.0f;
	float SpawnDistanceMax = 2000.0f;
	float HighLevelEnrageThreshold = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Enemies")
	TSubclassOf<ADREnemy> NormalMonsterClass;

	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Enemies")
	TSubclassOf<ADREnemy> RushMonsterClass;

	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Enemies")
	TSubclassOf<ADREnemy> StealthMonsterClass;

	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Enemies")
	TSubclassOf<ADREnemy> EliteBossClass;

	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Enemies")
	TSubclassOf<ADRPoisonGasActor> PoisonGasActorClass;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> EnemySpawnPoints;

	// 한 배치(4개 포인트)에서 아직 사용하지 않은 인덱스 목록
	TArray<int32> RemainingEnemySpawnPointIndices;

	UPROPERTY()
	int32 CurrentWaveNumber = 0;

	UPROPERTY()
	int32 CurrentWaveLevel = 1;

	UPROPERTY()
	EWaveState CurrentWaveState = EWaveState::Waiting;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> EliteBosses;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> ToxicGasActors;

	TArray<int32> ActiveSpawnPointIndices;
	TArray<int32> ActiveBlueSpawnPointIndices;

	FTimerHandle WaveTimerHandle;
	FTimerHandle SpawnTimerHandle;
	FTimerHandle DefenseTimerHandle;
	FTimerHandle PoisonGasSpawnTimerHandle;

	int32 CurrentSpawnTick = 0;
	int32 TotalSpawnTick = 0;
	int32 CurrentSpawnCount = 0;
	int32 TotalSpawnCount = 0;

	// MonsterSpawnCycle에서 현재 읽을 인덱스
	int32 CurrentSpawnCycleIndex = 0;

	bool bEliteBossSpawned = false;

	// ========== VFX ==========
	// 일반 적 스폰 포인트에 표시할 나이아가라 에셋
	UPROPERTY(EditDefaultsOnly, Category = "Phase3|VFX")
	TObjectPtr<UNiagaraSystem> EnemySpawnPointNiagaraSystem;

	// 엘리트 보스 스폰 포인트에 표시할 나이아가라 에셋
	UPROPERTY(EditDefaultsOnly, Category = "Phase3|VFX")
	TObjectPtr<UNiagaraSystem> EliteSpawnPointNiagaraSystem;

	// 엘리트 스폰 포인트 VFX 표시 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Phase3|VFX")
	float EliteSpawnVFXDuration = 3.0f;

	FTimerHandle EliteSpawnVFXTimerHandle;

	TMap<TObjectPtr<ADRCleanserSite>, bool> CleanserSiteHalfHealthTriggered;

	float DefenseStartTime = 0.0f;
	FTimerHandle WaveTimerUpdateHandle;
	float WaveTimeRemaining = 0.0f;
	float RestTimeRemaining = 0.0f;
};
