// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacterBase.h"
#include "DREnemy.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API ADREnemy : public ADRCharacterBase
{
	GENERATED_BODY()
	
public:
	ADREnemy();

	/** Combat Interface */
	virtual int32 GetPlayerLevel() override;
	/** end Combat Interface */

protected:
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	int32 Level = 1;
};
