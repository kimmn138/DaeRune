// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DRProjectile.h"
#include "DRSeedProjectile.generated.h"

class UDRSeedCannon;

/**
 * 시드 캐논 전용 발사체 클래스
 */
UCLASS()
class DAERUNE_API ADRSeedProjectile : public ADRProjectile
{
	GENERATED_BODY()
	
public:
    ADRSeedProjectile();

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

    UFUNCTION(NetMulticast, Reliable) // Unreliable이 아니라 Reliable로!
    void MulticastExplodeAtLocation(const FVector& ImpactLocation);

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

    // 폭발 중복 실행 방지 플래그
    bool bHasExploded = false;
};
