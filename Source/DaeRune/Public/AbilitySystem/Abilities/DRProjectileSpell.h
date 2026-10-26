// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "DRProjectileSpell.generated.h"

class ADRProjectile;
class UGameplayEffect;
struct FGameplayTag;

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRProjectileSpell : public UDRDamageGameplayAbility
{
	GENERATED_BODY()
	
public:
	// 업그레이드 칩(SkillProjectileCount)이 반영된 투사체 수. 최소 1발은 보장한다.
	UFUNCTION(BlueprintPure, Category = "Projectile")
	int32 GetEffectiveNumProjectiles() const;

	/**
	 * ★Plan5(동전칩)★ 칩이 반영된 투사체 크기 배율. 칩이 없으면 정확히 1.0.
	 *
	 * SpawnProjectile() 이 스폰 트랜스폼의 Scale3D 로 쓴다 — 외형(메시)과 충돌 구(Sphere)가 함께 커진다.
	 * 스폰 경로가 따로 있는 파생 GA(자판기 잭팟 캡슐)는 이 함수를 직접 불러 같은 값을 넣는다.
	 */
	UFUNCTION(BlueprintPure, Category = "Projectile|Upgrade")
	float GetUpgradedProjectileScale() const;

protected:

	UFUNCTION(BlueprintCallable, Category = "Projectile")
	ADRProjectile* SpawnProjectile(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag, bool bOverridePitch = false, float PitchOverride = 0.f);

	/**
	 * ★Plan8(FP)★ 스폰 위치를 직접 주는 버전 — SpawnProjectile 은 소켓 위치를 구해 이걸 부른다.
	 * 소켓 태그로 표현할 수 없는 위치(벨보이 모자의 FP 메시 고정 소켓 — Plan8 D-39)에서 쏠 때 쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	ADRProjectile* SpawnProjectileAt(const FVector& SpawnLocation, const FVector& ProjectileTargetLocation, bool bOverridePitch = false, float PitchOverride = 0.f);

	// 카메라 시점 라인트레이스로 조준점(크로스헤어 지점) 계산 — 투사체 계열 GA 공용 유틸
	UFUNCTION(BlueprintPure, Category = "Projectile")
	FVector CalculateTargetLocation() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<ADRProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly)
	int32 NumProjectiles = 5;
};
