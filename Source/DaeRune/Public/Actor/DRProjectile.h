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
 * �⺻ �߻�ü Ŭ����
 */
UCLASS()
class DAERUNE_API ADRProjectile : public AActor
{
	GENERATED_BODY()
	
public:
	ADRProjectile();

	// �߻�ü ���� �̵� ������Ʈ
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// ������ �� ����Ʈ �Ķ����
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true))
	FDamageEffectParams DamageEffectParams;

	// ���� �̻��Ͽ� Ÿ�� ������Ʈ
	UPROPERTY()
	TObjectPtr<USceneComponent> HomingTargetSceneComponent;

protected:
	virtual void BeginPlay() override;
	// �浹/Ÿ�� �� ����Ʈ ��� �� ����
	virtual void OnHit();
	// ���� �ı� �� ���� ����
	virtual void Destroyed() override;

	// �浹 ���� �� ������ ����
	UFUNCTION()
	virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 서버: Target 에게 이 발사체의 피해(넉백 확률 · 사망 임펄스 포함)를 건다.
	// ★팀 판정은 하지 않는다 — 호출부 책임★. 칩 지식은 없다(적 투사체와 공유 클래스) (Plan5(공기탄칩) D-94 · T-106)
	void ApplyImpactDamage(AActor* Target);

	// �浹 ������ ��ü ������Ʈ
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USphereComponent> Sphere;

	// Ÿ�� ���� �÷��� (���� Ŭ�������� ���� ��� ��������)
	bool bHit = false;

private:
	// �ڵ� �Ҹ� �ð�
	UPROPERTY(EditDefaultsOnly)
	float LifeSpan = 15.f;

	// �浹 �� ����� ���̾ư��� ����Ʈ
	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	// �浹 �� ����� ����
	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundBase> ImpactSound;

	// ���� �� �ݺ� ����� ����
	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundBase> LoopingSound;

	// �ݺ� ���� ������Ʈ
	UPROPERTY()
	TObjectPtr<UAudioComponent> LoopingSoundComponent;
};
