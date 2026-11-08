// Copyright DaeRune

#include "Character/DRCharacterMovementComponent.h"
#include "GameFramework/Character.h"

void UDRCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// PhysFalling 은 Velocity 의 수직 성분을 빼고(수평만) Acceleration 을 공중 가속으로 바꿔 둔 채 여기를 부른다.
	// 방향만 입력 쪽으로 돌리고 속력은 그대로 — 이어지는 Super 는 공중 마찰 0 이라 방향을 되돌리지 않고 공중 가속만 더한다
	if (ShouldTurnAirVelocityToInput(DeltaTime))
	{
		Velocity = Acceleration.GetSafeNormal() * Velocity.Size();
	}

	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

bool UDRCharacterMovementComponent::ShouldTurnAirVelocityToInput(float DeltaTime) const
{
	if (!bInstantAirTurnWhileJumping || MovementMode != MOVE_Falling || Acceleration.IsZero())
	{
		return false;
	}

	// Super::CalcVelocity 가 속도를 건드리지 않는 경우(루트 모션 · 시뮬레이션 프록시)는 같이 건너뛴다
	if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME
		|| CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return false;
	}

	// 내 점프로 뜬 공중만 — 땅에서 맞은 넉백 · 공기포 반동 · 낭떠러지 낙하는 관성 그대로.
	// JumpCurrentCount 는 세이브드 무브에 실려 클라 예측 · 재생과 서버가 같은 값을 본다 (클라 전용 플래그면 보정이 반복된다)
	if (CharacterOwner->JumpCurrentCount <= 0)
	{
		return false;
	}

	// 이동 속도를 넘는 속도(점프 중 맞은 큰 넉백 · 반동)도 관성 그대로 — Super 의 "최대 속도 초과" 판정과 같은 기준이라
	// 여기서 돌린 경우 Super 는 감속 쪽으로 가지 않는다
	const float MaxInputSpeed = FMath::Max(GetMaxSpeed() * AnalogInputModifier, GetMinAnalogSpeed());
	return !IsExceedingMaxSpeed(MaxInputSpeed);
}
