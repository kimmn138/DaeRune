// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "ScalableFloat.h"
#include "DRS2MoleClawAttack.generated.h"

class ADRS2MoleSlashWave;

/**
 * 두더지 보스 기본공격 — 45도 대각 검기 2발 (Plan7 §17)
 *
 * 몽타주 1개 안의 AnimNotify 2개가 PerformClawSweep(0) / PerformClawSweep(1) 을 호출한다.
 * 각 호출이 검기 1발을 발사하며, ★HitIndex 가 곧 대각선 방향 인덱스다 — 1타는 `/`, 2타는 `\`.
 *
 * ★원래는 전방 120도 부채꼴 즉발 판정이었다. 보스가 이동하지 않는(MOVE_None) 설계라
 *   사거리 밖에 서 있기만 하면 기본공격이 영구히 무의미해져 원거리 투사체로 전환했다.
 *   부채꼴 경로는 bUseMeleeSweep 으로 되살릴 수 있게 남겨 두었다 (A/B 비교 및 롤백용).
 *
 * ★데미지 축은 "구간(1/2/3차)"이다 (Plan7 §2.5-C). 인원수와는 무관하다.
 *   그래서 상속받은 Damage(= 어빌리티 레벨 = 인원수로 조회됨)를 쓰지 않고,
 *   구간을 직접 커브 레벨로 넘기는 전용 FScalableFloat 두 개를 둔다.
 *   ★이 축은 원거리 전환 후에도 그대로다 — 커브 행(CT_Damage)을 손댈 필요가 없다.
 */
UCLASS()
class DAERUNE_API UDRS2MoleClawAttack : public UDRDamageGameplayAbility
{
	GENERATED_BODY()

public:
	UDRS2MoleClawAttack();

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/**
	 * 몽타주 AnimNotify 에서 호출. HitIndex: 0 = 1타, 1 = 2타.
	 * ★BP(GA_Mole_Claw)가 부르는 진입점이라 이름·시그니처를 바꾸지 않는다 — 그래프 무수정이 전환 조건이었다.
	 */
	UFUNCTION(BlueprintCallable, Category = "MoleBoss|Claw")
	void PerformClawSweep(int32 HitIndex);

	/** 실제 근접 판정 반경 (캡슐 표면 + ClawReach). ★레거시 부채꼴 경로 전용. */
	UFUNCTION(BlueprintPure, Category = "MoleBoss|Claw (Legacy)")
	float GetEffectiveSweepRadius() const;

protected:
	// ===================== 검기 (기본 경로) =====================

	/** 발사할 검기 클래스. ★비어 있으면 아무 공격도 나가지 않는다 (부여 시점에 Error 로그). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|SlashWave")
	TSubclassOf<ADRS2MoleSlashWave> SlashWaveClass;

	/** 비행 속도(uu/s). 회피 반응 시간을 결정하는 값이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|SlashWave", meta = (ClampMin = "1.0"))
	float WaveSpeed = 1200.f;

	/**
	 * 최대 사거리(uu). 검기 수명이 여기서 자동 계산된다.
	 * ★BT 의 거리 데코레이터(권장 1300 = 사거리 - 100)와 함께 조정해야 한다 — BT 는 이 값을 모른다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|SlashWave", meta = (ClampMin = "1.0"))
	float WaveMaxRange = 1400.f;

	/**
	 * 발사 원점을 캡슐 반지름의 몇 배만큼 앞으로 밀지.
	 *
	 * ★1.0 이상으로 두면 안 된다. 이 보스는 캡슐 반지름이 커서 플레이어가 아무리 붙어도
	 *   중심에서 (보스반지름 + 플레이어반지름) 안쪽으로 못 들어온다. 원점을 캡슐 밖에 두면
	 *   밀착한 플레이어가 원점보다 뒤에 놓여 **영구 안전지대**가 생긴다.
	 *   몸통 안에서 출발시켜 캡슐 표면을 뚫고 나가게 하는 것이 의도된 동작이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|SlashWave", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float MuzzleForwardRatio = 0.6f;

	/**
	 * 발사 고도 — 보스 액터 Z 기준 오프셋.
	 * ★보스는 반매몰이라 액터 Z 가 곧 지면 Z 다. 이 값이 대각 칼날의 회전 중심 높이가 되므로
	 *   올리면 위쪽 사각이 넓어지고 아래쪽은 "무조건 맞음"이 된다 (회피 난이도 손잡이).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|SlashWave")
	float MuzzleHeight = 100.f;

	/**
	 * 전방 기준 조준 보정 한계(도). 0 = 순수 전방, 180 = 완전 조준.
	 * ★보스 회전이 느려서(RotationInterpSpeed 4.0) 순수 전방이면 스트레이프 상대로 영구히 빗나가고,
	 *   완전 조준이면 회피가 불가능해진다. 그 사이 값이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|SlashWave", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxAimYawFromForward = 25.f;

	/**
	 * ★대각선 기울기(Roll, 도). 인덱스 = HitIndex. 1타 `/` = +45, 2타 `\` = -45.
	 *
	 * 배열이 짧으면 마지막 값으로 폴백하고, 비어 있으면 0(수평)이 된다.
	 * ★게임에서 기울기가 반대로 보이면 **코드가 아니라 이 두 값을 맞바꾼다.**
	 *   기준 시점은 "플레이어가 검기를 정면으로 마주 본 화면"이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|SlashWave")
	TArray<float> SlashRollAngles;

	/** 1타당 발사 수. 2 이상이면 FanSpreadAngle 로 좌우 분산한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|SlashWave", meta = (ClampMin = "1"))
	int32 WavesPerHit = 1;

	/** 다발 발사 시 전체 분산 각도(도). WavesPerHit = 1 이면 무시된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|SlashWave", meta = (ClampMin = "0.0"))
	float FanSpreadAngle = 20.f;

	/**
	 * 조준 대상 후보에서 열차 좌석 착석자를 제외할지.
	 * ★착석자도 검기에 "맞기는" 한다 (경로상에 있으면). 다만 일부러 겨누지는 않는다 —
	 *   보스가 열차 쪽으로 몸을 돌리는 그림을 막기 위함이다 (스킬1의 bExcludeSeatedPlayers 와 같은 취지).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|SlashWave")
	bool bExcludeSeatedPlayersForAim = true;

	// ===================== 레거시 근접 부채꼴 =====================

	/** ★true 면 원거리 검기 대신 예전 근접 부채꼴로 동작한다 (A/B 비교 · 롤백용). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Claw (Legacy)")
	bool bUseMeleeSweep = false;

	/**
	 * 부채꼴 사거리 — 캡슐 **표면**에서 팔이 닿는 거리다 (중심 기준 절대값이 아니다).
	 * 실제 판정 반경 = GetScaledCapsuleRadius() + ClawReach.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Claw (Legacy)", meta = (ClampMin = "1.0"))
	float ClawReach = 150.f;

	/** 부채꼴 전체 각도(도). 120 = 전방 좌우 각 60도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Claw (Legacy)", meta = (ClampMin = "1.0", ClampMax = "360.0"))
	float SweepAngle = 120.f;

	/** 오버랩 구를 판정 반경보다 얼마나 크게 잡을지 (반매몰 Z 차이 보정). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Claw (Legacy)", meta = (ClampMin = "0.0"))
	float VerticalTolerance = 400.f;

	// ===================== 데미지 (경로 공통) =====================

	/**
	 * 1타 데미지. ★커브 레벨 = 구간 + 1 (1차→1, 2차→2, 3차→3).
	 * ★Value 는 반드시 1 로 둔다 — 결과가 Value * Curve[Level] 이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Claw")
	FScalableFloat Hit1Damage;

	/** 2타 데미지. 커브 레벨 규칙은 Hit1Damage 와 같다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Claw")
	FScalableFloat Hit2Damage;

private:
	/** 검기 발사 (기본 경로) */
	void FireSlashWaves(int32 HitIndex);

	/** 예전 근접 부채꼴 판정 (bUseMeleeSweep 전용) */
	void PerformMeleeSweep_Legacy(int32 HitIndex);

	/** 발사 원점 — 몸통 안 + 플레이어 가슴 높이 */
	FVector ResolveMuzzleLocation() const;

	/** 발사 Yaw — 전방 기준 ±MaxAimYawFromForward 로 클램프한 조준각 */
	float ResolveFireYaw(const FVector& MuzzleLocation) const;

	/** HitIndex → 대각선 기울기(Roll) */
	float ResolveSlashRoll(int32 HitIndex) const;

	/** 조준 대상 — 전투 타깃 우선, 없으면 최근접 생존 플레이어 */
	AActor* ResolveAimTarget(const FVector& MuzzleLocation) const;

	float ResolveDamage(int32 HitIndex) const;
};
