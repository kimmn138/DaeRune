// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DRCharacterMovementComponent.generated.h"

/**
 * 플레이어 이동 컴포넌트 (ADRCharacter 가 기본 CharacterMovement 대신 쓴다)
 *
 * 점프 중 공중 방향 전환: 엔진 기본은 공중에서 마찰(FallingLateralFriction)이 0 이라 속도 방향이 그대로 남고,
 * 공중 조작(AirControl 0.35 × 가속 2048 ≈ 717/s²)으로만 천천히 꺾여 달리기 점프 뒤 반대로 돌아서는 데 1.5초 넘게 걸렸다.
 * 여기서는 방향키를 누르는 동안 속도 방향을 바로 입력 쪽으로 돌린다 (속력은 그대로).
 */
UCLASS()
class DAERUNE_API UDRCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	/**
	 * 점프 중 방향키를 바꾸면 공중에서도 바로 그 방향으로 간다 (속력은 유지). 끄면 엔진 기본 관성(AirControl 로만 천천히 꺾임).
	 * 내 점프로 뜬 공중에서만 — 넉백 · 반동 · 낭떠러지 낙하, 이동 속도를 넘는 속도는 관성 그대로 둔다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Jumping / Falling")
	bool bInstantAirTurnWhileJumping = true;

	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

private:
	bool ShouldTurnAirVelocityToInput(float DeltaTime) const;
};
