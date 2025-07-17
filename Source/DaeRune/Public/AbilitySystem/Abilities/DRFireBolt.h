// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRProjectileSpell.h"
#include "DRFireBolt.generated.h"

/**
 * 파이어볼 프로젝타일 스펠 클래스 선언
 */
UCLASS()
class DAERUNE_API UDRFireBolt : public UDRProjectileSpell
{
	GENERATED_BODY()
	
public:
	/**
	 * 파이어볼 프로젝타일 생성 함수
	 * @param ProjectileTargetLocation 목표 위치 벡터
	 * @param SocketTag 소켓 태그
	 * @param bOverridePitch 피치 오버라이드 여부
	 * @param PitchOverride 피치 오버라이드 값
	 * @param HomingTarget 호밍 타겟 액터
	 */
	UFUNCTION(BlueprintCallable)
	void SpawnProjectiles(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag, bool bOverridePitch, float PitchOverride, AActor* HomingTarget);

protected:
	// 프로젝타일 스프레드 각도 변수
	UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
	float ProjectileSpread = 90.f;

	// 최대 생성 프로젝타일 수 변수
	UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
	int32 MaxNumProjectiles = 5;

	// 호밍 가속도 최소값 변수
	UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
	float HomingAccelerationMin = 1600.f;

	// 호밍 가속도 최대값 변수
	UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
	float HomingAccelerationMax = 3200.f;

	// 호밍 프로젝타일 발사 여부 변수
	UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
	bool bLaunchHomingProjectiles = true;
};
