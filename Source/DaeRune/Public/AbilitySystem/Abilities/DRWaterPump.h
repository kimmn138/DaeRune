// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "DRWaterPump.generated.h"

class UNiagaraComponent;

/**
 * ★Plan5(물대포칩)★ 이번 채널링에 적용되는 업그레이드 칩 스위치 한 벌.
 *
 * StartWaterPumpLoop() 에서 1회 해석해 캐시한다 — 채널링 중 로드아웃이 바뀔 일이 없고,
 * 매 틱(초당 10회) · 매 빔 갱신(초당 30회)마다 PlayerState 를 되짚는 낭비를 없앤다.
 *
 * ⚠️ Plan5(씨앗칩)의 FDRSeedBombResolved 와 달리 ★서버와 소유 클라이언트 양쪽에서 채운다.★
 *    관통(bPierce)이 빔 길이 계산을 바꾸는데, 그 계산은 클라이언트의 1P 빔도 하기 때문이다.
 *    클라가 모르면 "판정은 뚫는데 이펙트는 첫 적에서 잘리는" 상태가 된다. (D-14)
 */
USTRUCT()
struct FDRWaterPumpResolved
{
	GENERATED_BODY()

	bool bRamp       = false;   // ① 지속 명중 시 피해 누적
	bool bSlow       = false;   // ② 피격 적 감속
	bool bVulnerable = false;   // ② 피격 적이 받는 피해 증가
	bool bPierce     = false;   // ③ 적 관통
};

/**
 *
 */
UCLASS()
class DAERUNE_API UDRWaterPump : public UDRDamageGameplayAbility
{
	GENERATED_BODY()

public:
    // 1. 무기 소켓에서 에임 방향으로 레이트레이싱
    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    FVector CalculateWaterBeamEndPoint(const FVector& WeaponSocketLocation, bool& bHitObstacle, FHitResult& OutHitResult);

    // 2. 물대포 경로 상의 모든 적 찾기 (BoxOverlap)
    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    AActor* FindClosestTargetInBeam(const FVector& WeaponSocketLocation, const FVector& BeamEndPoint);

    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    void StartWaterPumpLoop();

    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    void StopWaterPumpLoop();

    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    void PerformWaterPumpTick();

    // 현재 타겟 관리
    UFUNCTION(BlueprintPure, Category = "Water Pump")
    AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

    UFUNCTION(BlueprintPure, Category = "Water Pump")
    bool HasValidTarget() const { return CurrentTarget.IsValid(); }

    UFUNCTION(BlueprintPure, Category = "Water Pump")
    int32 GetDamageTickCount() const { return DamageTickCounter; }

    UFUNCTION(BlueprintPure, Category = "Water Pump")
    FVector GetBeamEndPoint() const { return CachedBeamEndPoint; }

    // 블루프린트 이벤트 (헤더에 추가)
    UFUNCTION(BlueprintImplementableEvent, Category = "Water Pump")
    void OnTargetChanged(AActor* OldTarget, AActor* NewTarget);

    UFUNCTION(BlueprintImplementableEvent, Category = "Water Pump")
    void OnDamageTickReached(AActor* Target);

    UFUNCTION(BlueprintImplementableEvent, Category = "Water Pump")
    void OnBeamEndPointUpdated(const FVector& EndPoint);

    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    FGameplayAbilityTargetDataHandle MakeTargetDataHandleFromActors(AActor* TargetActor);

    /**
     * ★Plan5(물대포칩)★ 아군 1명에게 물을 지급한다. ★서버에서만 유효★
     *
     * BP 의 OnDamageTickReached 아군 분기에서 BP_ApplyGameplayEffectToTarget 대신 이걸 부른다.
     * 지급량 = ApplySkill(SkillWaterGrant, AllyWaterGrantAmount) 이라 칩이 자동 반영된다
     * (칩 ① → ×0, 칩 ② → ×2).
     */
    UFUNCTION(BlueprintCallable, Category = "Water Pump")
    void GrantWaterToAlly(AActor* AllyActor);

protected:
    // ★Plan5(물대포칩)★ 가압 스택(Buff.WaterPump.Pressure) 수에 비례한 피해 배율.
    virtual float GetRuntimeDamageMultiplier() const override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

    // Niagara System Asset
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    TObjectPtr<UNiagaraSystem> WaterCannonEffect;

    // 채널링 중 자기 자신에게 적용되는 슬로우 GE (서버에서 적용/제거)
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    TSubclassOf<UGameplayEffect> SlowSelfEffectClass;

    // 슬로우 GE 핸들 (Stop/EndAbility에서 제거하기 위해 보관)
    FActiveGameplayEffectHandle ActiveSlowSelfHandle;

    // 발사 소켓 이름
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    FName MuzzleSocketName = FName("TestRightHand");

    // 1P Niagara Component (3P는 DRCharacter에서 리플리케이트 상태로 관리)
    UPROPERTY()
    TObjectPtr<UNiagaraComponent> FirstPersonBeam;

    // 타이머
    FTimerHandle BeamUpdateTimer;

    // 함수들
    void StartBeamEffect();
    void UpdateBeamEndpoint();
    void StopBeamEffect();

    // 무기 사정거리
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float WeaponRange = 1000.f;

    // 물대포 폭
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float BeamWidth = 25.f;

    // 물대포 높이
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float BeamHeight = 25.f;

    // Tick 간격 (0.1초)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float TickInterval = 0.1f;

    // 데미지 적용 간격 (1초 = 10 ticks)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    int32 DamageApplicationInterval = 10;

    // 디버그 시각화 활성화
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    bool bShowDebugVisualization = false;

    // ========== Plan5(물대포칩) 어빌리티 기본값 ==========
    // 칩과 무관한 "물대포 자체의 기준값"이라 ChipTuning 이 아니라 여기에 둔다.

    /**
     * 아군 1명에게 1회 지급하는 물의 양(기본값). 칩은 SkillWaterGrant 로 이 값에 곱해진다.
     * ★GE_WaterPump_GrantWater 에 박혀 있던 ScalableFloat 값을 그대로 옮겨 적을 것.★
     * 안 옮기면 칩이 없을 때의 급수량이 조용히 바뀐다(회귀).
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    float AllyWaterGrantAmount = 10.f;

    /**
     * 아군 급수에 쓰는 GE. Instant + Water 에 SetByCaller(Water.SetByCaller.Grant) Additive.
     * GE_WaterPump_GrantWater 를 SetByCaller 로 바꿔 지정한다.
     * (적 처치 물 지급용 GE_WaterGrant 를 그대로 지정해도 사양이 같아 동작한다)
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump")
    TSubclassOf<UGameplayEffect> AllyWaterGrantEffect;

    /**
     * ★Plan5(물대포칩)★ 관통 시 한 틱에 때릴 수 있는 최대 대상 수(안전 상한).
     * 관통 빔의 반복 트레이스 횟수 상한으로도 같이 쓴다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Pump", meta = (ClampMin = "1"))
    int32 MaxPierceTargets = 8;

    // 현재 타겟과 이전 타겟 추적
    UPROPERTY(BlueprintReadOnly, Category = "Water Pump")
    TWeakObjectPtr<AActor> CurrentTarget;

    UPROPERTY(BlueprintReadOnly, Category = "Water Pump")
    TWeakObjectPtr<AActor> PreviousTarget;

    // 캐시된 물대포 끝점 (블루프린트에서 이펙트 위치로 사용)
    UPROPERTY(BlueprintReadOnly, Category = "Water Pump")
    FVector CachedBeamEndPoint;

private:
    FTimerHandle WaterPumpTimerHandle;

    // 단일 카운터
    int32 DamageTickCounter = 0;

    // 카메라 에임 방향 계산
    bool GetAimDirection(FVector& OutAimStart, FVector& OutAimDirection) const;

    // ========== Plan5(물대포칩) ==========

    /** 이번 채널링의 칩 스위치. StartWaterPumpLoop 에서 1회 해석한다. */
    FDRWaterPumpResolved Resolved;

    /**
     * ① 가압 스택 GE 의 핸들.
     *
     * ★이 핸들의 StackCount 가 스택 수다. ASC 의 태그 카운트가 아니다.★
     * GAS 는 Stacking 이 켜진 GE 를 ★하나의 액티브 GE 로 합치고 StackCount 만 올린다.★
     * 부여 태그(Buff.WaterPump.Pressure)는 그 액티브 GE 하나에 대해 1회만 더해지므로
     * GetGameplayTagCount() 는 스택이 몇 개든 항상 1 이다.
     *
     * (칩 ⑦ 시너지가 "태그 카운트 = 스택 수"로 동작하는 것은 그 GE 가 Stacking = None 이라
     *  적용마다 ★별개 인스턴스★ 가 생기기 때문이다 — 설정이 다르면 세는 법도 달라진다)
     *
     * 만료/제거로 핸들이 무효해지면 GetCurrentStackCount 가 0 을 돌려주므로 별도 정리가 필요 없다.
     */
    FActiveGameplayEffectHandle RampStackHandle;

    /** 장착 칩을 읽어 Resolved 를 채운다. ★서버/클라 분기보다 먼저★ 불러야 한다. (D-14) */
    void ResolveUpgradedParams();

    /**
     * 빔 상자 안의 유효 대상(살아 있고 ICombatInterface 를 구현)을 ★거리순으로★ 모은다.
     * ★아군도 포함된다★ — 적/아군 분기는 BP 의 OnDamageTickReached 가 한다.
     */
    void CollectTargetsInBeam(const FVector& WeaponSocketLocation, const FVector& BeamEndPoint,
        TArray<AActor*>& OutTargets) const;

    /**
     * 칩 규칙으로 대상 목록을 좁힌다.
     *  - 관통 없음: 가장 가까운 하나만 (= 기존 동작)
     *  - 관통 있음: 적은 전부 + ★아군은 가장 가까운 1명만★ + 안전 상한 (D-18)
     */
    void NarrowTargetsToChipRules(TArray<AActor*>& Targets) const;

    /**
     * 빔을 막는 것을 찾는다. 관통이면 캐릭터형 액터를 건너뛴다.
     * Params 를 ★값으로★ 받는 이유: 무시 목록을 늘려 가며 재트레이스하므로 호출부를 더럽히면 안 된다.
     */
    bool TraceBeamBlocker(const FVector& Start, const FVector& End, bool bPierceCharacters,
        FCollisionQueryParams Params, FHitResult& OutHit) const;

    /** ★D-16★ 피해보다 ★먼저★ 거는 칩 디버프 (감속 · 취약). 적에게만 걸린다. */
    void ApplyChipDebuffs(const TArray<AActor*>& Targets);

    /** 목록에 유효한 적이 하나라도 있는가. ★반드시 피해를 넣기 전에★ 판정해야 한다. */
    bool HasEnemyTarget(const TArray<AActor*>& Targets) const;

    /** ① 적을 하나라도 맞힌 틱이면 가압 스택 1개 추가 + 지속시간 갱신. ★틱당 1개★ */
    void AdvanceRampStack(bool bTickHitEnemy);

    /** ① 가압 스택 전부 제거 (물대포를 끌 때) */
    void ClearRampStacks();
};
