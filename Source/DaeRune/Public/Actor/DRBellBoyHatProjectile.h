// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DRProjectile.h"
#include "DRBellBoyHatProjectile.generated.h"

class ADRBellBoyHatPickup;
class URotatingMovementComponent;
class UStaticMeshComponent;

/**
 * ★Plan8(모자)★ 날아가는 모자.
 * 부모(ADRProjectile)의 이동·수명·임팩트 연출을 그대로 쓰고 "무엇에 닿았나"만 다시 판정한다:
 *   자신·시체·트리거·다른 투사체 → 무시 / 적 → 피해 후 소멸 / 아군 → 모자(없으면) 또는 그 자리 픽업(있으면)
 *   / 폰을 막는 표면(벽·바닥·클렌저 사이트) → 그 자리 바닥에 픽업
 */
UCLASS()
class DAERUNE_API ADRBellBoyHatProjectile : public ADRProjectile
{
	GENERATED_BODY()

public:
	ADRBellBoyHatProjectile();

protected:
	virtual void BeginPlay() override;

	virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	/** 날아가는 모자 외형 (충돌 없음). 회전은 SpinComponent 가 이 메시만 돌린다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BellBoy|Hat")
	TObjectPtr<UStaticMeshComponent> HatMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BellBoy|Hat")
	TObjectPtr<URotatingMovementComponent> SpinComponent;

	/** 벽·바닥·모자 쓴 아군에 닿았을 때 생기는 픽업 */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hat")
	TSubclassOf<ADRBellBoyHatPickup> PickupClass;

	/** 픽업을 바닥에서 얼마나 띄울지(uu) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hat")
	float PickupGroundOffset = 5.f;

private:
	AActor* GetThrower() const;
	void HandleEnemyHit(AActor* Target);
	void DropAsPickup(const FVector& FromLocation);

	// 한 번만 처리 (캡슐·히트박스가 같은 프레임에 여러 번 겹친다)
	bool bResolved = false;
};
