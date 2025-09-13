// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacterBase.h"
#include "Interaction/EnemyInterface.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "DREnemy.generated.h"

class UWidgetComponent;
class UBehaviorTree;
class ADRAIController;

/**
 * 
 */
UCLASS()
class DAERUNE_API ADREnemy : public ADRCharacterBase, public IEnemyInterface
{
	GENERATED_BODY()
	
public:
	ADREnemy();
	virtual void PossessedBy(AController* NewController) override;

	/** Combat Interface */
	virtual int32 GetPlayerLevel_Implementation() override;
	virtual void Die(const FVector& DeathImpulse) override;
	virtual void SetCombatTarget_Implementation(AActor* InCombatTarget) override;
	virtual AActor* GetCombatTarget_Implementation() const override;
	/** end Combat Interface */

	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	TObjectPtr<AActor> CombatTarget;

	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnMaxHealthChanged;

	// Water System
	UFUNCTION(BlueprintCallable, Category = "Water System")
	void OnAttackExecuted();

	void HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bHitReacting = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float LifeSpan = 5.f;

	// 넉백 상태 설정
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetKnockbackState(bool bInKnockback);

protected:
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo() override;
	virtual void InitializeDefaultAttributes() const override;
	virtual void StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	int32 Level = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UWidgetComponent> HealthBar;

	UPROPERTY(EditAnywhere, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	UPROPERTY()
	TObjectPtr<ADRAIController> DRAIController;

	// Water System Configuration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water System", meta = (ClampMin = "0.0"))
	float WaterReductionPerAttack = -10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water System", meta = (ClampMin = "100.0"))
	float WaterExplosionRadius = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water System")
	bool bIsBoss = false;

	UPROPERTY(EditDefaultsOnly, Category = "Water System")
	TSubclassOf<UGameplayEffect> WaterReductionEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Water System")
	TSubclassOf<UGameplayEffect> WaterGrantEffectClass;

	// ========== 벽 충돌 기절 시스템 추가 ==========

	// Hit 이벤트 핸들러
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse,
		const FHitResult& Hit);

	// 벽 스턴 적용 함수
	void ApplyWallStun();

	// 스턴 면역 종료
	void EndStunImmunity();

	// 넉백 상태
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Wall Stun")
	bool bIsBeingKnockedBack = false;

	// 충돌 강도 임계값
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Wall Stun", meta = (ClampMin = "100.0", ClampMax = "2000.0"))
	float MinSpeedForStun = 50.f;

	// 스턴 지속 시간
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Wall Stun", meta = (ClampMin = "0.5", ClampMax = "5.0"))
	float WallStunDuration = 5.0f;

	// 스턴 면역 시간 (스턴 종료 후)
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Wall Stun", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float StunImmunityDuration = 5.0f;

private:
	void ReduceWaterReward();
	void GrantWaterToPlayers();

	UPROPERTY()
	int32 AttackCount = 0;

	// 스턴 면역 상태
	bool bIsStunImmune = false;

	// 스턴 면역 타이머
	FTimerHandle StunImmunityTimerHandle;
};
