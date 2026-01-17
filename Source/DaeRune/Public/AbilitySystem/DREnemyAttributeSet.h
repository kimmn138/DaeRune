// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "DREnemyAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API UDREnemyAttributeSet : public UDRAttributeSet
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff")
	float EliteBuffModifier = 0.7f;

private:
	virtual void HandleIncomingDamage(const FEffectProperties& Props) override;
	virtual void HandleIncomingHealing(const FEffectProperties& Props) override;
};
