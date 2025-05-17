// Fill out your copyright notice in the Description page of Project Settings.

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

protected:
	virtual void BeginPlay() override;
};
