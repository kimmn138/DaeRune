// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "Interaction/CombatInterface.h"
#include "Interaction/DRProximityHitOnly.h"
#include "DRS2Mole.generated.h"

class UAbilitySystemComponent;
class UCapsuleComponent;
class ADRS2MoleGame;

/**
 * 홀로그램 두더지 (Plan6 §14.4)
 *
 * 방4 바닥에서 튀어나오는 표적. 2m 이내로 근접해서 공격해야 사라지고,
 * 2m 밖에서의 공격은 무시(투과)된다. 일정 시간 공격받지 않으면 스스로 사라진다.
 *
 * 구현 방식:
 *  - 액터 태그 "Enemy" 를 달아 IsNotFriend(액터 태그 기반)를 통과시킨다.
 *  - 최소 ASC 를 갖는다. 투사체가 GetAbilitySystemComponent(대상) 으로 ASC 를 찾지 못하면
 *    ApplyDamageEffect 가 아예 호출되지 않으므로 ASC 자체는 필요하다.
 *  - ★AttributeSet 은 두지 않는다. 대신 IDRProximityHitOnly 로 피격을 직접 처리한다.
 *    (ApplyDamageEffect 진입부에서 가로채므로 GE 가 적용되지 않는다 - §5.11)
 *  - 이동/AI 가 없는 순수 표적이다.
 */
UCLASS()
class DAERUNE_API ADRS2Mole : public AActor, public IAbilitySystemInterface, public ICombatInterface, public IDRProximityHitOnly
{
	GENERATED_BODY()

public:
	ADRS2Mole();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ===== ICombatInterface =====
	//
	// ★어트리뷰트를 쓰지 않는 대상이지만 이 인터페이스는 구현해야 한다 (2026-08-27).
	//   프로젝트 곳곳의 대상 수집이 Implements<UCombatInterface>() 로 후보를 거르기 때문에
	//   (GetLiveObjectsWithinRadius, DRWaterPump 등) 미구현이면 아예 대상으로 잡히지 않는다.
	//   실제 피격 처리는 여전히 IDRProximityHitOnly 가 담당하며,
	//   ApplyDamageEffect 진입부에서 가로채므로 GE 는 적용되지 않는다(크래시 위험 없음).
	virtual void Die(const FVector& DeathImpulse) override;
	virtual FOnDeathSignature& GetOnDeathDelegate() override { return OnDeathDelegate; }
	virtual FOnASCRegistered& GetOnASCRegisteredDelegate() override { return OnASCRegisteredDelegate; }

	virtual bool IsDead_Implementation() const override { return bVanishing; }
	virtual AActor* GetAvatar_Implementation() override { return this; }
	virtual FVector GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag) override { return GetActorLocation(); }

	// IDRProximityHitOnly
	virtual bool AcceptsHitFrom(const AActor* Attacker) const override;
	virtual void HandleProximityHit(AActor* Attacker) override;

	// 게임 관리자가 스폰 직후 호출한다 (유지 시간 주입 + 등장 연출)
	void InitFromGame(ADRS2MoleGame* InGame, float InLifetime);

	UFUNCTION(BlueprintCallable, Category = "S2|Mole")
	float GetProximityRadius() const { return ProximityRadius; }

protected:
	virtual void BeginPlay() override;

	// ICombatInterface 요구 델리게이트 (바인딩 대상이 필요해 보유만 한다)
	FOnDeathSignature OnDeathDelegate;
	FOnASCRegistered OnASCRegisteredDelegate;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Mole")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Mole")
	TObjectPtr<UStaticMeshComponent> MoleMesh;

	// 투사체/트레이스에 걸리는 콜리전
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Mole")
	TObjectPtr<UCapsuleComponent> HitBox;

	// 최소 ASC (어트리뷰트 없음 - 파이프라인이 대상을 찾을 수 있게 하기 위한 용도)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Mole")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// 유효 피격 반경. **플레이어 액터 ↔ 두더지 액터** 직선거리로 잰다.
	//
	// 원안은 2m(200uu)였으나 350 으로 상향했다 (2026-08-27).
	// 정원사 물대포처럼 노즐이 몸에서 앞으로 뻗은 무기는 빔이 두더지에 닿아도
	// 몸통 거리가 200 을 넘어 판정이 거부되는 일이 잦았다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Mole", meta = (ClampMin = "0.0"))
	float ProximityRadius = 350.f;

	// 소멸 연출 시간 (이 시간이 지나면 Destroy)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Mole", meta = (ClampMin = "0.0"))
	float VanishDuration = 0.3f;

	// 소멸 진행 중 여부 (중복 피격/만료 방지 + 연출 트리거)
	UPROPERTY(ReplicatedUsing = OnRep_bVanishing, BlueprintReadOnly, Category = "S2|Mole")
	bool bVanishing = false;

	UFUNCTION()
	void OnRep_bVanishing();

	// 바닥에서 튀어나오는 연출
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Mole")
	void OnEmergeVisual();

	// 홀로그램 소멸 연출
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Mole")
	void OnVanishVisual(bool bWasKilled);

private:
	// 유지 시간 만료
	void HandleLifetimeExpired();

	// 소멸 시작 (서버). bKilled = 플레이어가 때려서 사라진 경우
	void BeginVanish(bool bKilled);

	// 연출 후 실제 제거
	void FinishVanish();

	UPROPERTY()
	TWeakObjectPtr<ADRS2MoleGame> OwningGame;

	FTimerHandle LifetimeTimer;
	FTimerHandle VanishTimer;

	// 소멸 원인 (연출 구분용)
	bool bKilledByPlayer = false;
};
