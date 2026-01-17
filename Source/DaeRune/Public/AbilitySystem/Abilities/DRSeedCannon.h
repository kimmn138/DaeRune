// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRProjectileSpell.h"
#include "DRSeedCannon.generated.h"

class ADRSeedProjectile;

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRSeedCannon : public UDRProjectileSpell
{
	GENERATED_BODY()
	
public:
    // 발사체 스폰 함수 - 블루프린트에서 호출
    UFUNCTION(BlueprintCallable, Category = "SeedCannon")
    void SpawnSeedProjectile(const FVector& ForwardVector, const FGameplayTag& SocketTag);

protected:
    // SeedCannon 전용 발사체 클래스
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon")
    TSubclassOf<ADRSeedProjectile> SeedProjectileClass;

    // 발사 속도
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon")
    float LaunchSpeed = 500.f;

    // 발사 각도 (수평 기준 위로)
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon")
    float LaunchAngle = 35.f;
};
