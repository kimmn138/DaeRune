// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DRAIController.generated.h"

class UBlackboardComponent;
class UBehaviorTreeComponent;

/**
 * DREnemy 전용 AI 컨트롤러
 */
UCLASS()
class DAERUNE_API ADRAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	ADRAIController();

protected:
	// 비헤이비어 트리 실행 컴포넌트
	UPROPERTY()
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;
};
