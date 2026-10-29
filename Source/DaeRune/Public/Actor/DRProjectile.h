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

	// 서버: Target 에게 이 발사체의 피해(넉백 확률 · 사망 임펄스 포함)를 건다.
	// ★팀 판정은 하지 않는다 — 호출부 책임★. 칩 지식은 없다(적 투사체와 공유 클래스) (Plan5(공기탄칩) D-94 · T-106)
	void ApplyImpactDamage(AActor* Target);

	// 충돌 감지용 구체 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USphereComponent> Sphere;

	// 적중 여부 (파생 클래스에서도 쓰도록 protected)
	bool bHit = false;

private:
	// 자동 소멸 시간
	UPROPERTY(EditDefaultsOnly)
	float LifeSpan = 15.f;

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
