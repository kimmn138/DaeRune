// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRBellBoyHatPickup.generated.h"

class UAbilitySystemComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * ★Plan8(모자)★ 바닥에 떨어진 모자. LifeTime(10초) 뒤 사라진다.
 * 닿은 캐릭터는 적이든 아군이든 모자를 쓴다(모자가 없을 때만). 던진 벨보이 본인이면 2회 (Plan8 D-23).
 * 판정은 서버만, 액터는 복제된다(모든 화면에 보인다).
 */
UCLASS()
class DAERUNE_API ADRBellBoyHatPickup : public AActor
{
	GENERATED_BODY()

public:
	ADRBellBoyHatPickup();

	/** 스폰 직후(FinishSpawning 전) 투사체가 부른다. 서버 전용 값이라 복제하지 않는다 */
	void InitThrower(AActor* InThrower, UAbilitySystemComponent* InThrowerASC);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnPickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 이미 겹쳐 있던 대상 / 모자를 잃고 그 자리에 서 있는 대상을 위해 주기적으로 다시 본다 (Plan8 D-25)
	void RecheckOverlaps();
	void TryGiveTo(AActor* Candidate);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BellBoy|Hat")
	TObjectPtr<USphereComponent> PickupSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BellBoy|Hat")
	TObjectPtr<UStaticMeshComponent> HatMesh;

	/** 바닥에 남아 있는 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hat", meta = (ClampMin = "0.1"))
	float LifeTime = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hat", meta = (ClampMin = "0.05"))
	float RecheckInterval = 0.25f;

	/** 던진 벨보이 본인이 주웠을 때 막는 횟수 */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hat", meta = (ClampMin = "1"))
	int32 ThrowerCharges = 2;

private:
	TWeakObjectPtr<AActor> Thrower;
	TWeakObjectPtr<UAbilitySystemComponent> ThrowerASC;
	FTimerHandle RecheckTimerHandle;
	bool bConsumed = false;
};
