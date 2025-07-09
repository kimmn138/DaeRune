// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DRAIController.generated.h"

class UBlackboardComponent;
class UBehaviorTreeComponent;

/**
 * 
 */
UCLASS()
class DAERUNE_API ADRAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	ADRAIController();

protected:
	UPROPERTY()
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;
};
