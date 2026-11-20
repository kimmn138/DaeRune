// Copyright DaeRune

#include "AI/DRBTTask_FaceTarget.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "GameFramework/Pawn.h"

UDRBTTask_FaceTarget::UDRBTTask_FaceTarget()
{
	NodeName = TEXT("Face Target");
	INIT_TASK_NODE_NOTIFY_FLAGS();

	// 액터와 위치만 받는다
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UDRBTTask_FaceTarget, BlackboardKey), AActor::StaticClass());
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UDRBTTask_FaceTarget, BlackboardKey));
}

uint16 UDRBTTask_FaceTarget::GetInstanceMemorySize() const
{
	return sizeof(FDRFaceTargetMemory);
}

void UDRBTTask_FaceTarget::InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const
{
	InitializeNodeMemory<FDRFaceTargetMemory>(NodeMemory, InitType);
}

void UDRBTTask_FaceTarget::CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const
{
	CleanupNodeMemory<FDRFaceTargetMemory>(NodeMemory, CleanupType);
}

EBTNodeResult::Type UDRBTTask_FaceTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FDRFaceTargetMemory* Memory = CastInstanceNodeMemory<FDRFaceTargetMemory>(NodeMemory);
	*Memory = FDRFaceTargetMemory();

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	if (!Pawn) return EBTNodeResult::Failed;

	FVector TargetLocation;
	AActor* TargetActor = nullptr;
	if (!GetTarget(OwnerComp, TargetLocation, TargetActor)) return EBTNodeResult::Failed;

	// 이미 보고 있으면 기다리지 않는다 — 공격 사이 대기 동안 ADRAIController 의 기본 포커스로 대개 돌아 있다
	if (IsFacing(*Pawn, TargetLocation)) return EBTNodeResult::Succeeded;

	SetFocusTo(*AIController, *Memory, TargetActor, TargetLocation);
	return EBTNodeResult::InProgress;
}

void UDRBTTask_FaceTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FDRFaceTargetMemory* Memory = CastInstanceNodeMemory<FDRFaceTargetMemory>(NodeMemory);

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;

	FVector TargetLocation;
	AActor* TargetActor = nullptr;
	if (!Pawn || !GetTarget(OwnerComp, TargetLocation, TargetActor))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (IsFacing(*Pawn, TargetLocation))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	Memory->ElapsedTime += DeltaSeconds;
	if (Memory->ElapsedTime >= MaxDuration)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 도는 사이 블랙보드 대상이 바뀌면(가장 가까운 플레이어 교체 등) 새 대상을 본다
	SetFocusTo(*AIController, *Memory, TargetActor, TargetLocation);
}

void UDRBTTask_FaceTarget::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	// 성공 · 실패 · 중단(기절 데코레이터 등) 모두 여기로 온다
	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		ClearOwnFocus(*AIController, *CastInstanceNodeMemory<FDRFaceTargetMemory>(NodeMemory));
	}

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

FString UDRBTTask_FaceTarget::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: %s\n정면 ±%.0f° 안에 들 때까지 (최대 %.1f초, 넘으면 실패)"),
		*Super::GetStaticDescription(), *BlackboardKey.SelectedKeyName.ToString(), Precision, MaxDuration);
}

bool UDRBTTask_FaceTarget::GetTarget(const UBehaviorTreeComponent& OwnerComp, FVector& OutLocation, AActor*& OutActor) const
{
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard) return false;

	if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
	{
		OutActor = Cast<AActor>(Blackboard->GetValue<UBlackboardKeyType_Object>(BlackboardKey.GetSelectedKeyID()));
		if (!OutActor) return false;

		OutLocation = OutActor->GetActorLocation();
		return true;
	}

	if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
	{
		OutLocation = Blackboard->GetValue<UBlackboardKeyType_Vector>(BlackboardKey.GetSelectedKeyID());
		return FAISystem::IsValidLocation(OutLocation);
	}

	return false;
}

bool UDRBTTask_FaceTarget::IsFacing(const APawn& Pawn, const FVector& TargetLocation) const
{
	// 수평 방향만 본다 — 적은 Yaw 만 돈다
	const FVector ToTarget = (TargetLocation - Pawn.GetActorLocation()).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero()) return true;	// 바로 위 · 아래면 돌 방향이 없다

	const FVector Forward = Pawn.GetActorForwardVector().GetSafeNormal2D();
	return FVector::DotProduct(Forward, ToTarget) >= FMath::Cos(FMath::DegreesToRadians(Precision));
}

void UDRBTTask_FaceTarget::SetFocusTo(AAIController& AIController, FDRFaceTargetMemory& Memory, AActor* TargetActor, const FVector& TargetLocation) const
{
	if (TargetActor)
	{
		if (Memory.bFocusSet && Memory.bActorSet && Memory.FocusActorSet.Get() == TargetActor) return;

		AIController.SetFocus(TargetActor, EAIFocusPriority::Gameplay);
		Memory.FocusActorSet = TargetActor;
		Memory.bActorSet = true;
	}
	else
	{
		if (Memory.bFocusSet && !Memory.bActorSet && Memory.FocusLocationSet.Equals(TargetLocation)) return;

		AIController.SetFocalPoint(TargetLocation, EAIFocusPriority::Gameplay);
		Memory.FocusLocationSet = TargetLocation;
		Memory.bActorSet = false;
	}
	Memory.bFocusSet = true;
}

void UDRBTTask_FaceTarget::ClearOwnFocus(AAIController& AIController, FDRFaceTargetMemory& Memory) const
{
	if (!Memory.bFocusSet) return;

	const bool bStillOurs = Memory.bActorSet
		? (Memory.FocusActorSet.IsValid() && AIController.GetFocusActorForPriority(EAIFocusPriority::Gameplay) == Memory.FocusActorSet.Get())
		: AIController.GetFocalPointForPriority(EAIFocusPriority::Gameplay).Equals(Memory.FocusLocationSet);

	if (bStillOurs)
	{
		AIController.ClearFocus(EAIFocusPriority::Gameplay);
	}
	Memory.bFocusSet = false;
}
