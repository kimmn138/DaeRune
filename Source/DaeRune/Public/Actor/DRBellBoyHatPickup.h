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
 *
 * ★Plan5(모자칩)★ 수명 · 2회 규칙은 "던진 벨보이의 모자칩"을 따른다 — ②면 모든 플레이어 2회, ③이면 수명 18초.
 * 모자 던지기가 떨군 것이든 기본 공격(가방칩②)이 떨군 것이든 같다 (Plan5 D-128 · D-134)
 */
UCLASS()
class DAERUNE_API ADRBellBoyHatPickup : public AActor
{
	GENERATED_BODY()

public:
	ADRBellBoyHatPickup();

	/**
	 * ★Plan5(가방칩 · 모자칩)★ 바닥 모자를 세우는 유일한 길 — 서버 전용 (Plan5 D-127).
	 * FromLocation 아래 바닥을 ★오브젝트 타입(WorldStatic/Dynamic)★ 으로 찾아(Plan8 T-17) GroundOffset 만큼 띄운다.
	 * Deferred 스폰 사이에 InitThrower 를 불러 칩 규칙(수명 · 2회)을 BeginPlay 전에 정한다 (Plan5 T-142).
	 * @param IgnoreActor 바닥 트레이스가 무시할 액터(던진 모자 자신 등). 없어도 된다 — 트레이스는 Pawn 을 안 본다
	 */
	static ADRBellBoyHatPickup* SpawnOnGround(UWorld* World, TSubclassOf<ADRBellBoyHatPickup> PickupClass,
		const FVector& FromLocation, float Yaw, AActor* InThrower, UAbilitySystemComponent* InThrowerASC,
		const AActor* IgnoreActor = nullptr, float GroundOffset = 5.f);

	/**
	 * 스폰 직후(FinishSpawning 전) 부른다. 서버 전용 값이라 복제하지 않는다.
	 * ★Plan5(모자칩)★ 던진 벨보이의 칩 규칙(수명 · 2회)도 여기서 1회 해석해 둔다 (Plan5 D-128)
	 */
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

	/** 던진 벨보이 본인이 주웠을 때 막는 횟수 (★Plan5(모자칩)★ ②면 모든 플레이어) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hat", meta = (ClampMin = "1"))
	int32 ThrowerCharges = 2;

private:
	TWeakObjectPtr<AActor> Thrower;
	TWeakObjectPtr<UAbilitySystemComponent> ThrowerASC;
	FTimerHandle RecheckTimerHandle;
	bool bConsumed = false;

	// ★Plan5(모자칩)★ ③ 이 모자의 수명(초) — InitThrower 가 정한다. 음수면 LifeTime 그대로 (Plan5 D-128)
	float EffectiveLifeTime = -1.f;

	// ★Plan5(모자칩)★ ② 던진 벨보이가 칩②면 플레이어 전원이 ThrowerCharges 로 줍는다 (Plan5 D-134)
	bool bShareDoubleCharges = false;
};
