// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "DRAbilityTypes.h"
#include "GameFramework/Actor.h"
#include "DRProjectile.generated.h"

class UNiagaraSystem;
class USphereComponent;
class UProjectileMovementComponent;

/**
 * 기본 발사체 클래스
 */
UCLASS()
class DAERUNE_API ADRProjectile : public AActor
{
	GENERATED_BODY()
	
public:
	ADRProjectile();

	// 발사체 물리 이동 컴포넌트
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// 데미지 및 이펙트 파라미터
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true))
	FDamageEffectParams DamageEffectParams;

	// 유도 미사일용 타겟 컴포넌트
	UPROPERTY()
	TObjectPtr<USceneComponent> HomingTargetSceneComponent;

protected:
	virtual void BeginPlay() override;
	// 충돌/타격 시 이펙트 재생 및 정리
	virtual void OnHit();
	// 액터 파괴 시 사운드 정리
	virtual void Destroyed() override;

	// 충돌 감지 및 데미지 적용
	UFUNCTION()
	virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 충돌 감지용 구체 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USphereComponent> Sphere;

private:
	// 자동 소멸 시간
	UPROPERTY(EditDefaultsOnly)
	float LifeSpan = 15.f;

	// 타격 상태 플래그
	bool bHit = false;

	// 충돌 시 재생할 나이아가라 이펙트
	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	// 충돌 시 재생할 사운드
	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundBase> ImpactSound;

	// 비행 중 반복 재생할 사운드
	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundBase> LoopingSound;

	// 반복 사운드 컴포넌트
	UPROPERTY()
	TObjectPtr<UAudioComponent> LoopingSoundComponent;
};
