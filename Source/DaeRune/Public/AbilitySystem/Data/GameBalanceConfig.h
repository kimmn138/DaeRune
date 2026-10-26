// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Phase/DRPhase3DataTypes.h"
#include "GameBalanceConfig.generated.h"

class UGameplayEffect;

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

	// 비전투 자동 회복량 (회복 GE 의 주기 1회당, MMC_HealthRegen)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float HealthRegenPerTick = 10.f;

	// 물이 모자랄 때 부족분을 체력으로 낼 비율 (부족분 × 이 값, 내림)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float WaterShortageHealthRatio = 0.5f;
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

	// 엘리트 포효 오라(Buff.Elite.Roar)를 받은 적이 주는 피해 배율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1.0", ClampMax = "5.0"))
	float EliteRoarDamageMultiplier = 1.2f;

	// 확보 페이즈(Phase1) 적이 플레이어 · 적에게 주는 피해 배율 (클렌저 사이트 제외)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Phase1DamageMultiplier = 0.7f;
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
 * ★Plan8(모자)★ 벨보이 모자 보호막 설정.
 * 피해 관문(1회 무효화) · 모자 투사체(아군 장착) · 픽업(주운 대상 장착)이 전부 여기를 본다 (Plan8 D-27).
 * ★서버에서만 읽는다★ — GameMode 의 DA 라 클라이언트에는 없다. 세 곳 모두 서버 경로라 문제없다.
 */
USTRUCT(BlueprintType)
struct FBellBoyConfig
{
	GENERATED_BODY()

	// 1회 모자 — Infinite / Target Tags = Buff.BellBoy.Hat.Single / Cue = GameplayCue.BellBoy.Hat.Single
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BellBoy|Hat")
	TSubclassOf<UGameplayEffect> HatSingleEffect;

	// 2회 모자(던진 벨보이 본인이 주웠을 때). 한 번 막으면 Single 로 바뀐다 (Plan8 D-20)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BellBoy|Hat")
	TSubclassOf<UGameplayEffect> HatDoubleEffect;
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

	// ========== 캐릭터 설정 ==========

	// 벨보이 (Plan8)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	FBellBoyConfig BellBoy;

	// ========== Phase3 설정 ==========

	// Phase3 (방어전) 전역 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase3")
	FPhase3Config Phase3;

	// ========== 헬퍼 함수 ==========

	// 물 감소량 반환 (음수)
	float GetWaterReductionAmount() const { return -WaterSystem.WaterReductionPerAttack; }
};
