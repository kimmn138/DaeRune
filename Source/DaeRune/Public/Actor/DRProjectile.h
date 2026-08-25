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
