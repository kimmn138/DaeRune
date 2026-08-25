// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "DRS2Types.generated.h"

class ADREnemy;

// 퍼즐 해결 알림 (금고 비밀번호 한 자리를 공개하는 퍼즐이 서버에서 발화)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnS2PuzzleSolved, AActor*, Puzzle);

// 금고 개방 알림 (내부에 스폰된 부품을 전달)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnS2SafeOpened, AActor*, SpawnedPart);

/**
 * 스테이지2 공용 타입 (Plan6 §4.1)
 *
 * 스테이지2의 웨이브는 "인원별 룩업 테이블" 방식이다.
 * 배수 계산이 아니라 1~4인 각 구간의 구성을 BP 배열로 직접 지정한다.
 * 기준 인원은 방 시작 시점의 생존자 수로 1회 확정하고 이후 사망해도 재계산하지 않는다.
 */

/** 적 1종 x 마리 수 */
USTRUCT(BlueprintType)
struct FS2EnemyCount
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "S2|Spawn")
	TSubclassOf<ADREnemy> EnemyClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "S2|Spawn", meta = (ClampMin = "0"))
	int32 Count = 0;
};

/** 웨이브 1개 구성 */
USTRUCT(BlueprintType)
struct FS2WaveComposition
{
	GENERATED_BODY()

	// 이 웨이브에 스폰할 적 (종류별 마리 수)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "S2|Spawn", meta = (TitleProperty = "EnemyClass"))
	TArray<FS2EnemyCount> Enemies;

	// 방 전투 시작 시점 기준 스폰 지연. 방1은 [0초, 30초] 두 웨이브를 시간 기반으로 예약한다.
	// 방5처럼 페이즈가 전환 시점을 직접 제어하는 경우에는 사용하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "S2|Spawn", meta = (ClampMin = "0.0"))
	float StartDelaySeconds = 0.f;

	// 같은 웨이브 안에서 마리 단위 스폰 간격 (0 = 동시 스폰).
	// 적이 "막 쏟아져 나오는" 연출을 이 값으로 조절한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "S2|Spawn", meta = (ClampMin = "0.0"))
	float PerEnemySpawnInterval = 0.f;
};

/** 인원 1구간(1인/2인/3인/4인)의 웨이브 목록 */
USTRUCT(BlueprintType)
struct FS2WaveSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "S2|Spawn")
	TArray<FS2WaveComposition> Waves;
};
