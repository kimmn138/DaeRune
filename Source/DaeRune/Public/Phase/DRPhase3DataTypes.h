// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DRPhase3DataTypes.generated.h"

/**
 * 웨이브 데이터 DataTable Row
 * RowName: "Wave1", "Wave2", ... "Wave5"
 */
USTRUCT(BlueprintType)
struct FWaveDataRow : public FTableRowBase
{
	GENERATED_BODY()

	// 웨이브 플레이 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Wave")
	float PlayDuration = 50.0f;

	// 웨이브 휴식 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Wave")
	float RestDuration = 10.0f;

	// 기본 스폰 주기 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn")
	float BaseSpawnInterval = 5.0f;

	// 기본 플레이어당 몬스터 수
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn")
	int32 BaseMonstersPerPlayer = 8;
};

/**
 * 웨이브 레벨 수정자 DataTable Row
 * RowName: "Level1", "Level2", ... "Level5"
 */
USTRUCT(BlueprintType)
struct FWaveLevelModifierRow : public FTableRowBase
{
	GENERATED_BODY()

	// 몬스터 수 증가율 (1.0 = 100%, 1.2 = 120%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Multiplier")
	float MonsterCountMultiplier = 1.0f;

	// 스폰 간격 감소율 (1.0 = 100%, 0.8 = 80%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Multiplier")
	float SpawnIntervalMultiplier = 1.0f;

	// 돌진형 몬스터 스폰 확률 (0.0 ~ 1.0)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Special Enemies", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RushMonsterSpawnChance = 0.0f;

	// 은신형 몬스터 스폰 확률 (0.0 ~ 1.0)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Special Enemies", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StealthMonsterSpawnChance = 0.0f;

	// 유독 가스 생성 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hazards")
	bool bSpawnToxicGas = false;

	// 엘리트 보스 스폰 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Special Enemies")
	bool bSpawnEliteBoss = false;
};

/**
 * 독가스 스폰 그리드 설정
 */
USTRUCT(BlueprintType)
struct FPoisonGasGridConfig
{
	GENERATED_BODY()

	// 그리드 시작점 (좌하단 기준)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid")
	FVector GridOrigin = FVector::ZeroVector;

	// 가로(X) 간격
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "100.0"))
	float SpacingX = 500.0f;

	// 세로(Y) 간격
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "100.0"))
	float SpacingY = 500.0f;

	// 가로 개수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1", ClampMax = "20"))
	int32 CountX = 8;

	// 세로 개수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1", ClampMax = "20"))
	int32 CountY = 6;

	// 클렌저당 CleanserLinked 포인트 개수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1", ClampMax = "8"))
	int32 LinkedPointsPerCleanser = 4;

	// 총 그리드 포인트 수 반환
	int32 GetTotalCount() const { return CountX * CountY; }
};

/**
 * Phase3 전역 설정 (GameBalanceConfig에 포함)
 */
USTRUCT(BlueprintType)
struct FPhase3Config
{
	GENERATED_BODY()

	// 최대 몬스터 수 (이 이상이면 게임 오버)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Over", meta = (ClampMin = "10", ClampMax = "500"))
	int32 MaxMonsterCount = 100;

	// 방어 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Duration", meta = (ClampMin = "60.0", ClampMax = "600.0"))
	float DefenseDuration = 300.0f;

	// 몬스터 스폰 최소 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "100.0", ClampMax = "1000.0"))
	float SpawnDistanceMin = 500.0f;

	// 몬스터 스폰 최대 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "500.0", ClampMax = "5000.0"))
	float SpawnDistanceMax = 2000.0f;

	// 독가스 스폰 주기 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazards", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float PoisonGasSpawnInterval = 10.0f;

	// 레벨 4 이상 광폭화 체력 비율 (기본값과 다름)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enrage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HighLevelEnrageThreshold = 0.25f;
};
