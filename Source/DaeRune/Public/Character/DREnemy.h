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

private:
	void ReduceWaterReward();
	void GrantWaterToPlayers();

	UPROPERTY()
	int32 AttackCount = 0;
};
