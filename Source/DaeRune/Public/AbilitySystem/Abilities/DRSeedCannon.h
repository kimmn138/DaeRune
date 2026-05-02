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
    // 발사체 생성 함수 - 블루프린트에서 호출
    // SocketLocation: 클라이언트의 1인칭 메시에서 구한 소켓 월드 위치
    UFUNCTION(BlueprintCallable, Category = "SeedCannon")
    void SpawnSeedProjectile(const FVector& ForwardVector, const FVector& SocketLocation);

protected:
    // SeedCannon ���� �߻�ü Ŭ����
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon")
    TSubclassOf<ADRSeedProjectile> SeedProjectileClass;

    // �߻� �ӵ�
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon")
    float LaunchSpeed = 500.f;

    // �߻� ���� (���� ���� ����)
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon")
    float LaunchAngle = 35.f;
};
