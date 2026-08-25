// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DRPhase3DataTypes.generated.h"

/**
 * Wave data table row.
 * RowName: Wave1..Wave5
 */
USTRUCT(BlueprintType)
struct FWaveDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Wave")
	float PlayDuration = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Wave")
	float RestDuration = 10.0f;
};

/**
 * Wave level modifier table row.
 * RowName: Level1..Level5
 */
USTRUCT(BlueprintType)
struct FWaveLevelModifierRow : public FTableRowBase
{
	GENERATED_BODY()

	// Time between each spawn batch.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0.1"))
	float SpawnIntervalSeconds = 2.5f;

	// Total number of spawns = this value * player count.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "4"))
	int32 SpawnCycleLengthPerPlayer = 8;

	// Monster type cycle. 1=Normal, 2=Rush, 3=Stealth.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1", ClampMax = "3"))
	TArray<int32> MonsterSpawnCycle = { 1, 1, 1, 1, 1, 1, 1, 1 };

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hazards")
	bool bSpawnToxicGas = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Special Enemies")
	bool bSpawnEliteBoss = false;
};

/**
 * Global Phase3 config loaded from GameBalanceConfig.
 */
USTRUCT(BlueprintType)
struct FPhase3Config
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Over", meta = (ClampMin = "10", ClampMax = "500"))
	int32 MaxMonsterCount = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duration", meta = (ClampMin = "60.0", ClampMax = "600.0"))
	float DefenseDuration = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "100.0", ClampMax = "1000.0"))
	float SpawnDistanceMin = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "500.0", ClampMax = "5000.0"))
	float SpawnDistanceMax = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazards", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float PoisonGasSpawnInterval = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enrage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HighLevelEnrageThreshold = 0.25f;
};
