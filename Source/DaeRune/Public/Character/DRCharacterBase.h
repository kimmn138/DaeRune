// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DRCharacterBase.generated.h"

UCLASS()
class DAERUNE_API ADRCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ADRCharacterBase();

protected:
	virtual void BeginPlay() override;

};
