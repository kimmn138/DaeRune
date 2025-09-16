// Copyright DaeRune


#include "AI/DRAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

ADRAIController::ADRAIController()
{
	// 블랙보드 컴포넌트 생성 - AI 상태 데이터 저장소
	Blackboard = CreateDefaultSubobject<UBlackboardComponent>("BlackboardComponent");
	check(Blackboard);
	// 비헤이비어 트리 컴포넌트 생성 - AI 행동 로직 실행
	BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>("BehaviorTreeComponent");
	check(BehaviorTreeComponent);
}
