// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DRPhaseBase.generated.h"

// This class does not need to be modified.
class AStageManager;

UCLASS(BlueprintType, Blueprintable)
class UDRPhaseBase : public UObject
{
	GENERATED_BODY()

public:
	void Setup(AStageManager* InManager) { Manager = InManager; }

	virtual void Enter() {}
	virtual void Exit() {}
	virtual void Tick(float DeltaTime) {}

protected:
	UPROPERTY() AStageManager* Manager = nullptr;
};
