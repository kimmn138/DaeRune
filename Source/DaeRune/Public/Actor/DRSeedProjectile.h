// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DRProjectile.h"
#include "DRSeedProjectile.generated.h"

class UDRSeedCannon;

/**
 * 
 */
UCLASS()
class DAERUNE_API ADRSeedProjectile : public ADRProjectile
{
	GENERATED_BODY()
	
public:
    ADRSeedProjectile();

protected:
    virtual void BeginPlay() override;

    virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult) override;

    // 폭발 처리
    void ExplodeAtLocation(const FVector& ImpactLocation);

    // Line of Sight 체크 (벽 뒤에 있는지 확인)
    bool HasLineOfSight(const FVector& StartLocation, const FVector& EndLocation, const AActor* TargetActor) const;

    // Line of Sight 체크 활성화 여부
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|LineOfSight")
    bool bEnableLineOfSightCheck = true;  // 기본적으로 비활성화

    // 거리별 효과 적용
    void ApplyEffectToActor(AActor* Target, float Distance);

    // 힐 효과 적용 (아군용)
    void ApplyHealToAlly(AActor* AllyActor, float HealAmount);

private:
    // 범위 설정
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Range")
    float InnerRadius = 200.f;

    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Range")
    float OuterRadius = 400.f;

    // 데미지 설정 (적군)
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Damage")
    float InnerDamage = 70.f;

    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Damage")
    float OuterDamage = 30.f;

    // 힐 설정 (아군)
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Heal")
    float InnerHeal = 100.f;

    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Heal")
    float OuterHeal = 50.f;

    // 힐 이펙트 클래스
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon|Heal")
    TSubclassOf<UGameplayEffect> HealEffectClass;

    // 폭발 여부 체크
    bool bHasExploded = false;
};
