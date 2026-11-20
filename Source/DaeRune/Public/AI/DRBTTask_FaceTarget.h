// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "DRBTTask_FaceTarget.generated.h"

class AAIController;

/**
 * 블랙보드 대상(액터 · 위치)이 정면 ±Precision 안에 들 때까지 돌아선다 — 근접 공격 태스크 바로 앞에 둔다.
 *
 * "Get Close" MoveTo 는 대상이 도달 반경 안에 있으면 움직이지 않고 바로 성공한다. 등 뒤에 붙은 플레이어를
 * 그대로 두고 공격 태스크로 넘어가 허공을 물었다 — 이 태스크가 그 사이에서 먼저 돌려세운다.
 *
 * 돌리는 일은 적의 Tick(ControlRotation 을 따라 회전)이 한다. 여기서는 Gameplay 포커스만 걸고,
 * 끝날 때 자기가 건 포커스가 그대로면 지운다(엔진 RotateToFaceBBEntry 와 같은 방식).
 * MaxDuration 안에 못 돌면 실패한다 — 시퀀스가 끊기고 트리가 다시 골라 공격을 허공에 내지 않는다
 * (플레이어가 바짝 붙어 빙빙 돌면 적은 따라 돌기만 한다).
 */
UCLASS(meta = (DisplayName = "Face Target (DR)"))
class DAERUNE_API UDRBTTask_FaceTarget : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UDRBTTask_FaceTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual FString GetStaticDescription() const override;

	virtual uint16 GetInstanceMemorySize() const override;
	virtual void InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const override;
	virtual void CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const override;

protected:
	/** 정면에서 좌우 이 각도(도) 안에 들면 다 돈 것으로 본다 */
	UPROPERTY(EditAnywhere, Category = "Face", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float Precision = 30.f;

	/** 이 시간(초) 안에 못 돌면 실패 */
	UPROPERTY(EditAnywhere, Category = "Face", meta = (ClampMin = "0.1"))
	float MaxDuration = 1.f;

private:
	struct FDRFaceTargetMemory
	{
		TWeakObjectPtr<AActor> FocusActorSet;	// 이 태스크가 Gameplay 포커스로 건 액터
		FVector FocusLocationSet = FVector::ZeroVector;
		bool bFocusSet = false;
		bool bActorSet = false;
		float ElapsedTime = 0.f;
	};

	/** 블랙보드 값 → 바라볼 위치 (액터면 OutActor 도). 값이 비었으면 false */
	bool GetTarget(const UBehaviorTreeComponent& OwnerComp, FVector& OutLocation, AActor*& OutActor) const;

	bool IsFacing(const APawn& Pawn, const FVector& TargetLocation) const;

	/** 대상이 바뀌었으면 Gameplay 포커스를 다시 건다 */
	void SetFocusTo(AAIController& AIController, FDRFaceTargetMemory& Memory, AActor* TargetActor, const FVector& TargetLocation) const;

	/** 이 태스크가 건 포커스가 그대로면 지운다 (그 사이 다른 쪽이 Gameplay 포커스를 바꿨으면 두고 간다) */
	void ClearOwnFocus(AAIController& AIController, FDRFaceTargetMemory& Memory) const;
};
