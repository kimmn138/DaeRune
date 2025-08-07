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
	
public:

private:
	virtual void HandleIncomingDamage(const FEffectProperties& Props) override;
	virtual void HandleIncomingHealing(const FEffectProperties& Props) override;
};
