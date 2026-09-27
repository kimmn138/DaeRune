// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DRProjectile.h"
#include "DRSeedProjectile.generated.h"

class UDRSeedCannon;

/**
 * ★Plan5(씨앗칩)★ 업그레이드 칩이 반영된 폭발 수치 한 벌.
 *
 * BeginPlay(서버)에서 1회 계산해 캐시한다 — 비행 중 로드아웃이 바뀔 일이 없고,
 * 대상마다 PlayerState 를 되짚는 낭비를 없앤다.
 * 칩이 하나도 없으면 발사체의 기본값과 정확히 같다(항등).
 *
 * ⚠️ 서버에서만 채워진다. 클라이언트(시뮬레이션 프록시)에서는 기본값이 그대로 남으므로
 *    Multicast 함수 안에서 이 값을 쓰면 안 된다.
 */
USTRUCT()
struct FDRSeedBombResolved
{
    GENERATED_BODY()

    float InnerRadius = 200.f;
    float OuterRadius = 400.f;
    float InnerDamage = 70.f;
    float OuterDamage = 30.f;
    float InnerHeal   = 100.f;
    float OuterHeal   = 50.f;

    bool bRefundOnDirectHit = false;
    bool bSlowEnemies       = false;
    bool bHasteAllies       = false;
};

/**
 * 시드 캐논 전용 발사체 클래스
 */
UCLASS()
class DAERUNE_API ADRSeedProjectile : public ADRProjectile
{
	GENERATED_BODY()

public:
    ADRSeedProjectile();

    /**
     * ★Plan5(씨앗칩)★ 이 발사체를 쏘는 데 실제로 지불된 물의 양.
     * 직격 환급 칩(GR.Asc.SeedRefund)이 이 값을 근거로 돌려준다.
     * UDRSeedCannon::SpawnSeedProjectile 이 FinishSpawning 전에 채운다.
     *
     * ★복제하지 않는다★ — 환급은 서버에서만 계산·적용된다.
     */
    UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = "SeedCannon|Upgrade")
    float SpentWaterCost = 0.f;

protected:
    virtual void BeginPlay() override;

    // 충돌 감지 - 지형/액터와 접촉 시 폭발
    virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult) override;

    // 지정된 위치에서 범위 폭발 실행
    void ExplodeAtLocation(const FVector& ImpactLocation);

    // 시선 차단 검사 (벽 뒤의 대상 제외용)
    bool HasLineOfSight(const FVector& StartLocation, const FVector& EndLocation, const AActor* TargetActor) const;

    // 시선 차단 검사 활성화 여부
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|LineOfSight")
    bool bEnableLineOfSightCheck = true;  // 기본적으로 비활성화

    // 거리별 차등 효과 적용
    void ApplyEffectToActor(AActor* Target, float Distance);

    // 아군 대상 힐 효과 적용
    void ApplyHealToAlly(AActor* AllyActor, float HealAmount);

    /**
     * 폭발 VFX 를 모든 머신에서 재생한다.
     *
     * ★Plan5(씨앗칩)★ RadiusScale 은 "기본 범위 대비 몇 배인가"다 (칩이 없으면 1.0).
     * Resolved 는 서버에만 채워지므로 클라이언트가 범위 칩을 알 방법이 이 인자뿐이다.
     */
    UFUNCTION(NetMulticast, Reliable) // Unreliable이 아니라 Reliable로!
    void MulticastExplodeAtLocation(const FVector& ImpactLocation, float RadiusScale);

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayExplosionSound(const FVector& Location);

private:
    // 내부 범위 반경 (최대 효과)
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Range")
    float InnerRadius = 200.f;

    // 외부 범위 반경 (감소된 효과)
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Range")
    float OuterRadius = 400.f;

    // 내부 범위 데미지량
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Damage")
    float InnerDamage = 70.f;

    // 외부 범위 데미지량
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Damage")
    float OuterDamage = 30.f;

    // 내부 범위 힐량
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Heal")
    float InnerHeal = 100.f;

    // 외부 범위 힐량
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Heal")
    float OuterHeal = 50.f;

    // 힐 적용용 GameplayEffect 클래스
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Heal")
    TSubclassOf<UGameplayEffect> HealEffectClass;

    // 폭발 나이아가라 이펙트
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Effects")
    TObjectPtr<UNiagaraSystem> ExplosionEffect;

    /**
     * ★Plan5(씨앗칩)★ 범위 칩이 붙었을 때 폭발 VFX 도 같이 키울지.
     *
     * true  : 나이아가라 컴포넌트의 트랜스폼 스케일을 RadiusScale 로 곱한다. ★에셋 작업 0★
     *         단 월드 스페이스 이미터는 컴포넌트 스케일을 무시하므로 시스템 구성에 따라 안 먹을 수 있다.
     * false : 크기를 건드리지 않고 아래 User 파라미터만 넘긴다(아티스트가 직접 제어).
     *
     * 어느 쪽이든 나이아가라 시스템에 ★float User 파라미터 `RadiusScale`★ 을 만들어 두면
     * 그 값으로도 전달된다(1.0 = 기본 범위). 파라미터가 없으면 조용히 무시된다.
     */
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Effects")
    bool bScaleExplosionVFX = true;

    // 폭발 중복 실행 방지 플래그
    bool bHasExploded = false;

    // ========== 업그레이드 칩 (Plan5(씨앗칩)) ==========

    // 업그레이드가 반영된 폭발 수치. BeginPlay(서버)에서 1회 해석한다.
    FDRSeedBombResolved Resolved;

    // ★적에게 직접 맞아서★ 터졌는가. 바닥/벽/아군이면 false.
    bool bDirectHitOnEnemy = false;

    // 위 Resolved 를 채운다. 서버에서만 호출한다.
    void ResolveUpgradedParams();

    // 직격 환급 칩이 켜져 있을 때 시전자에게 물을 되돌려준다.
    void GrantWaterRefund();
};
