// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "DRAbilityTypes.h"
#include "GameFramework/Actor.h"
#include "DRProjectile.generated.h"

class UNiagaraSystem;
class USphereComponent;
class UProjectileMovementComponent;

// 투사체 액터 클래스 정의
UCLASS()
class DAERUNE_API ADRProjectile : public AActor
{
	GENERATED_BODY()
	
public:
	ADRProjectile();

	// 프로젝타일 이동 컴포넌트 포인터 변수 선언
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// 데미지 이펙트 파라미터 구조체 변수 선언
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true))
	FDamageEffectParams DamageEffectParams;

	// 호밍 대상 장면 컴포넌트 포인터 변수 선언
	UPROPERTY()
	TObjectPtr<USceneComponent> HomingTargetSceneComponent;

protected:
	virtual void BeginPlay() override;
	// 히트 처리 내부 메서드 선언
	void OnHit();
	// 액터 파괴 시 처리 메서드 재정의 선언
	virtual void Destroyed() override;

	// 스피어 오버랩 이벤트 콜백 함수 선언
	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 스피어 콜리전 컴포넌트 포인터 변수 선언
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USphereComponent> Sphere;

private:
	// 생존 지속 시간 변수 선언
	UPROPERTY(EditDefaultsOnly)
	float LifeSpan = 15.f;

	// 히트 여부 플래그 변수 선언
	bool bHit = false;

	// 임팩트 이펙트 시스템 포인터 변수 선언
	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	// 임팩트 사운드 변수 선언
	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundBase> ImpactSound;

	// 루핑 사운드 변수 선언
	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundBase> LoopingSound;

	// 루핑 사운드 컴포넌트 포인터 변수 선언
	UPROPERTY()
	TObjectPtr<UAudioComponent> LoopingSoundComponent;
};
