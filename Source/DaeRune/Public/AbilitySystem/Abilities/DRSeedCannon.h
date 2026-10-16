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

    /**
     * ★Plan5(공속칩)★ 공속 배율이 반영된 몽타주 재생 속도 = BaseMontagePlayRate × 공속 배율 (버프가 없으면 1.0).
     * BP 의 Play Montage(1P) · Play Montage and Wait(3P) ★양쪽★ Play Rate 핀에 연결한다 — 한쪽만 바꾸면 손과 몸이 따로 논다.
     * 몽타주 한 개 안에 발사 전(선딜)·후(후딜)가 다 들어 있어 전체 배속이 곧 선·후딜 단축이다 (Plan5 D-68).
     */
    UFUNCTION(BlueprintPure, Category = "SeedCannon")
    float GetAttackSpeedPlayRate() const;

protected:
    // ★Plan5(공속칩)★ 버프가 없을 때의 몽타주 재생 속도. BP 의 원래 Play Rate 핀 값(1.0)을 그대로 옮긴 것
    UPROPERTY(EditDefaultsOnly, Category = "SeedCannon", meta = (ClampMin = "0.01"))
    float BaseMontagePlayRate = 1.f;

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
