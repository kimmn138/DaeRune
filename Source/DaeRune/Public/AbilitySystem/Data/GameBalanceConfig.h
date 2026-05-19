// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Phase/DRPhase3DataTypes.h"
#include "GameBalanceConfig.generated.h"

/**
 * 플레이어 컨테이너 시스템 밸런스 설정
 */
USTRUCT(BlueprintType)
struct FPlayerContainerConfig
{
	GENERATED_BODY()

	// 컨테이너 개수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container", meta = (ClampMin = "1", ClampMax = "10"))
	int32 NumContainers = 4;

	// 컨테이너당 체력
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container", meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float ContainerHealth = 100.f;
};

/**
 * 플레이어 전투 밸런스 설정
 */
USTRUCT(BlueprintType)
struct FPlayerCombatConfig
{
	GENERATED_BODY()

	// 전투 이탈 판정 대기 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float CombatExitDelay = 5.0f;

	// 파트 드롭 쿨다운 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Part System", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float PartDropCooldown = 2.0f;
};

/**
 * 적 전투 밸런스 설정
 */
USTRUCT(BlueprintType)
struct FEnemyCombatConfig
{
	GENERATED_BODY()

	// 히트 리액션 중 이동 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "500.0"))
	float HitReactingMoveSpeed = 200.f;

	// 사망 후 생존 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float LifeSpan = 2.f;

	// 파트 드롭 힘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Part System", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
	float PartDropForce = 300.f;
};

/**
 * 적 광폭화 시스템 밸런스 설정
 */
USTRUCT(BlueprintType)
struct FEnemyEnrageConfig
{
	GENERATED_BODY()

	// 광폭화 발동 체력 비율 (0.0 ~ 1.0)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enrage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EnrageHealthThreshold = 0.2f;

	// 광폭화 시 공격 속도 배율 (낮을수록 빠름)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enrage", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float EnrageAttackSpeedMultiplier = 0.5f;
};

/**
 * 벽 스턴 시스템 밸런스 설정
 */
USTRUCT(BlueprintType)
struct FWallStunConfig
{
	GENERATED_BODY()

	// 스턴 발동에 필요한 최소 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wall Stun", meta = (ClampMin = "10.0", ClampMax = "2000.0"))
	float MinSpeedForStun = 50.f;

	// 벽 스턴 지속 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wall Stun", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float WallStunDuration = 5.0f;

	// 스턴 면역 지속 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wall Stun", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float StunImmunityDuration = 5.0f;
};

/**
 * 물 리소스 시스템 밸런스 설정
 */
USTRUCT(BlueprintType)
struct FWaterSystemConfig
{
	GENERATED_BODY()

	// 공격당 물 보상 감소량 (양수로 입력, 내부에서 음수 처리)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float WaterReductionPerAttack = 10.f;

	// 물 폭발 반경 (일반 적)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "100.0", ClampMax = "2000.0"))
	float WaterExplosionRadius = 500.f;
};

/**
 * 게임 밸런스 통합 설정 DataAsset
 *
 * 게임 내 모든 밸런스 관련 수치를 중앙에서 관리합니다.
 * 코드 재컴파일 없이 에디터에서 밸런스 조절이 가능합니다.
 */
UCLASS(BlueprintType)
class DAERUNE_API UGameBalanceConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// ========== 플레이어 설정 ==========

	// 플레이어 컨테이너 시스템 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player")
	FPlayerContainerConfig PlayerContainer;

	// 플레이어 전투 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player")
	FPlayerCombatConfig PlayerCombat;

	// ========== 적 설정 ==========

	// 적 기본 전투 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	FEnemyCombatConfig EnemyCombat;

	// 적 광폭화 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	FEnemyEnrageConfig EnemyEnrage;

	// ========== 시스템 설정 ==========

	// 벽 스턴 시스템 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Systems")
	FWallStunConfig WallStun;

	// 물 리소스 시스템 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Systems")
	FWaterSystemConfig WaterSystem;

	// ========== Phase3 설정 ==========

	// Phase3 (방어전) 전역 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase3")
	FPhase3Config Phase3;

	// ========== 헬퍼 함수 ==========

	// 물 감소량 반환 (음수)
	float GetWaterReductionAmount() const { return -WaterSystem.WaterReductionPerAttack; }
};
