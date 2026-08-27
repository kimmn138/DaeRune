// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "ScalableFloat.h"
#include "DRS2MoleBurrowStrike.generated.h"

class ADRCharacter;
class ADRS2GroundWarning;
class ADRS2MoleBoss;

/**
 * 두더지 보스 스킬1 — 굴착 강습 (Plan7 §4.3 / §2.3)
 *
 *  t=0.00  활성 + 쿨다운 커밋. BP 가 잠수 몽타주 재생
 *  t=0.60  [BurrowDuration] 무적 진입 · 무작위 플레이어 지정 · 착지점 계산 · 경고 스폰
 *  t=1.40  [WarningDuration 0.8] 경고 소멸
 *  t=1.80  [EruptDelayAfterWarning 0.4] ★융기 — 텔레포트 + AoE + 전기장 링 버퍼
 *  t=2.60  [EmergeRecoverDuration] 어빌리티 종료
 *
 * ★데미지 축은 "인원수", 쿨다운 축은 "구간"이다 (Plan7 §2.5-C).
 *   데미지: 두더지 보스는 Level 자체가 인원수라(페이즈가 지연 스폰 시 주입) 어빌리티 레벨도 인원수다.
 *           따라서 상속받은 Damage(FScalableFloat) 를 그대로 쓰면 인원 축이 공짜로 붙는다 — 전용 필드 없음.
 *   쿨다운: 축이 다르므로 구간을 커브 레벨로 직접 넘기는 전용 FScalableFloat 을 둔다.
 * ★융기 AoE 는 플레이어뿐 아니라 다른 적에게도 적중한다 → IsNotFriend 를 쓰지 않는다.
 * ★어떤 경로로 취소되든 EndAbility 에서 타이머·경고 액터·무적 상태를 전부 되돌린다.
 *   보스가 도망(StashBoss)갈 때 이 취소가 돌지 않으면 숨겨진 보스가 융기하는 버그가 난다.
 */
UCLASS()
class DAERUNE_API UDRS2MoleBurrowStrike : public UDRDamageGameplayAbility
{
	GENERATED_BODY()

public:
	UDRS2MoleBurrowStrike();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** ★구간별 쿨다운. 부모 UDRGameplayAbility 의 훅을 오버라이드한다 (Plan7 §5.4). */
	virtual float GetBaseCooldownDuration(const FGameplayAbilityActorInfo* ActorInfo) const override;

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/**
	 * 잠수 완료 시점. 무적 진입 + 타깃 지정 + 경고 스폰.
	 * BurrowDuration 타이머가 자동 호출하지만, 잠수 몽타주 끝에 AnimNotify 를 달아
	 * 더 정확한 타이밍에 호출해도 된다 (중복 호출은 래치로 무시된다).
	 */
	UFUNCTION(BlueprintCallable, Category = "MoleBoss|Burrow")
	void BeginBurrowPhase();

protected:
	// ---------- 타이밍 ----------

	/**
	 * 잠수 연출 시간(= 이 시간이 지나면 메시를 숨기고 타깃을 지정한다).
	 *
	 * ★보스에 BurrowMontage 가 설정돼 있으면 **몽타주 길이가 우선**한다.
	 *   에셋 길이와 이 숫자가 어긋나 "파고드는 중간에 사라지는" 버그를 원천 차단하기 위함이다.
	 *   여기 값은 몽타주가 없을 때만 쓰이는 폴백이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow", meta = (ClampMin = "0.0"))
	float BurrowDuration = 0.6f;

	/** 지면 경고가 떠 있는 시간 (사용자 확정 0.8초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow", meta = (ClampMin = "0.05"))
	float WarningDuration = 0.8f;

	/** 경고가 사라진 뒤 융기까지의 공백 (사용자 확정 0.4초). 0 이면 경고 종료와 동시에 융기. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow", meta = (ClampMin = "0.0"))
	float EruptDelayAfterWarning = 0.4f;

	/**
	 * 융기 후 어빌리티가 유지되는 시간. 이 동안 재발동이 막힌다.
	 * ★보스에 EmergeMontage 가 있으면 그 길이가 우선한다 (BurrowDuration 과 같은 규칙).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow", meta = (ClampMin = "0.0"))
	float EmergeRecoverDuration = 0.8f;

	// ---------- 타깃 ----------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow", meta = (ClampMin = "100.0"))
	float MaxTargetRange = 4000.f;

	/** 열차 좌석에 앉은 플레이어를 타깃 후보에서 뺄지. 열차 위로 솟아오르는 그림을 막는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow")
	bool bExcludeSeatedPlayers = true;

	/** 경고가 타깃을 따라다닐지. false = 지정 순간 위치에 고정 (사용자 확정, 회피 가능). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow")
	bool bWarningFollowsTarget = false;

	/** 추종 모드일 때 경고 위치 갱신 주기 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow", meta = (ClampMin = "0.016"))
	float WarningFollowInterval = 0.05f;

	// ---------- 연출 / 데미지 ----------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow")
	TSubclassOf<ADRS2GroundWarning> WarningActorClass;

	/**
	 * 쿨다운(초). ★커브 레벨 = 구간 + 1 (1차→1, 2차→2, 3차→3).
	 *
	 * 어빌리티 레벨은 인원수라 여기에 쓸 수 없다. 그래서 구간을 직접 넘긴다.
	 * 반환값은 부모 ApplyCooldown 이 SetByCaller(Data.Cooldown) 로 주입하므로
	 * 업그레이드 칩(SkillCooldown) 경로가 그대로 유지된다.
	 *
	 * ★Value 는 반드시 1 로 둔다 — 결과가 Value * Curve[Level] 이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow")
	FScalableFloat BurrowCooldown;

	/** 띄우기 상향 성분 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow")
	float LaunchZ = 700.f;

	/** 띄우기 방사 성분 (중심에서 바깥으로) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow")
	float LaunchRadial = 300.f;

	/** 융기가 다른 적에게도 적중하는지 (사용자 확정: true) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow")
	bool bHitOtherEnemies = true;

	/** 클로 어빌리티와 같은 이유의 수직 여유 (반매몰 보정) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow", meta = (ClampMin = "0.0"))
	float VerticalTolerance = 400.f;

	/**
	 * BP 연출 훅 — 잠수 시작 순간(t=0). **몽타주는 여기서 재생하지 않는다** (C++ 이 이미 재생함).
	 * 흙먼지 Niagara, 사운드, 카메라 셰이크 같은 FX 만 넣는다.
	 * ※ 융기 순간의 FX 는 보스의 OnEruptVisual 이 담당한다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "MoleBoss|Burrow")
	void K2_OnBurrowStarted();

private:
	ADRS2MoleBoss* GetMoleBoss() const;

	/** 유효한 타깃 후보가 하나라도 있는지 (활성 직전 판정 — 없으면 쿨다운 없이 취소) */
	bool HasAnyValidTarget() const;

	ADRCharacter* PickRandomTarget() const;

	/** 타깃 발밑 지면 + NavMesh 투영으로 실제로 솟아오를 수 있는 지점을 구한다 */
	bool ResolveEruptGround(const AActor* Target, FVector& OutGround) const;

	void HandleWarningFinished();
	void HandleWarningFollowTick();
	void HandleErupt();
	void HandleEmergeFinished();
	void ApplyEruptDamage(const FVector& Center);

	void ClearAllTimers();

	TWeakObjectPtr<ADRCharacter> CachedTarget;
	TWeakObjectPtr<ADRS2GroundWarning> WarningActor;

	FVector PendingEruptGround = FVector::ZeroVector;

	// BeginBurrowPhase 중복 진입 방지 (타이머 + AnimNotify 양쪽에서 호출될 수 있다)
	bool bBurrowPhaseStarted = false;

	FTimerHandle BurrowTimer;
	FTimerHandle WarningTimer;
	FTimerHandle EruptTimer;
	FTimerHandle EmergeTimer;
	FTimerHandle WarningFollowTimer;
};
