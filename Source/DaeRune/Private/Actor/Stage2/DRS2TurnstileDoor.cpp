// Copyright DaeRune

#include "Actor/Stage2/DRS2TurnstileDoor.h"

#include "DaeRune/DRLogChannels.h"

ADRS2TurnstileDoor::ADRS2TurnstileDoor()
{
	// 날개 메시는 BP 에서 태그를 달아 추가한다 (C++ 고정 컴포넌트를 두지 않는다).
}

void ADRS2TurnstileDoor::InitializePose()
{
	CollectLeaves();
}

void ADRS2TurnstileDoor::CollectLeaves()
{
	Leaves.Reset();

	CollectGroup(GroupATag, OpenRotationA);
	CollectGroup(GroupBTag, bMirrorGroupB ? OpenRotationA.GetInverse() : OpenRotationB);

	if (Leaves.Num() == 0)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2Turnstile] %s: 날개를 찾지 못했습니다. BP 컴포넌트에 '%s' 또는 '%s' 태그를 다세요."),
			*GetName(), *GroupATag.ToString(), *GroupBTag.ToString());
	}
}

void ADRS2TurnstileDoor::CollectGroup(FName Tag, const FRotator& OpenRotation)
{
	if (Tag.IsNone()) return;

	TArray<USceneComponent*> Components;
	GetComponents<USceneComponent>(Components);

	// 컴포넌트 이름 순으로 정렬해 배치 순서를 안정화한다 (연출 타이밍을 나중에 어긋나게 주고 싶을 때 대비)
	Components.Sort([](const USceneComponent& A, const USceneComponent& B)
	{
		return A.GetName() < B.GetName();
	});

	for (USceneComponent* Component : Components)
	{
		if (!Component || !Component->ComponentHasTag(Tag)) continue;

		FTurnstileLeaf Leaf;
		Leaf.Component = Component;

		// ★배치 당시의 상대 회전이 "닫힘 원점"이다. 닫히면 정확히 이 값으로 되돌아간다.
		Leaf.ClosedQuat = Component->GetRelativeRotation().Quaternion();

		// 열림 목표는 원점에 회전량을 **로컬로** 합성한 값이다.
		// (월드 축이 아니라 날개 자신의 축을 기준으로 돌아야 배치 방향과 무관하게 동작한다)
		Leaf.OpenQuat = Leaf.ClosedQuat * OpenRotation.Quaternion();

		Leaves.Add(Leaf);
	}
}

void ADRS2TurnstileDoor::ApplyPose(float Alpha)
{
	// Alpha 0 = 열림 포즈, 1 = 막힘(닫힘) 포즈
	// 베이스의 규약이 "1 = 막힘"이므로 닫힘 쪽이 1이다.
	const float OpenAmount = 1.f - FMath::Clamp(Alpha, 0.f, 1.f);

	for (const FTurnstileLeaf& Leaf : Leaves)
	{
		if (!Leaf.Component.IsValid()) continue;

		// 쿼터니언 보간으로 큰 각도에서도 자연스럽게 돈다
		const FQuat Current = FQuat::Slerp(Leaf.ClosedQuat, Leaf.OpenQuat, OpenAmount);
		Leaf.Component->SetRelativeRotation(Current.Rotator());
	}
}

void ADRS2TurnstileDoor::EnsureLeavesCollected()
{
	// ★이미 수집했으면 원점을 다시 읽지 않는다.
	//   열린 자세에서 재수집하면 그 자세를 새 원점으로 오인해 90도가 더 돌아간다.
	if (Leaves.Num() > 0) return;

	CollectLeaves();
}

void ADRS2TurnstileDoor::PreviewOpenPose()
{
	EnsureLeavesCollected();
	ApplyPose(0.f);
}

void ADRS2TurnstileDoor::PreviewClosedPose()
{
	EnsureLeavesCollected();
	ApplyPose(1.f);
}

void ADRS2TurnstileDoor::ResetPreviewOrigin()
{
	// 날개를 다시 배치했을 때 쓴다. 반드시 **닫힌 자세**에서 눌러야 한다.
	Leaves.Reset();
	CollectLeaves();

	UE_LOG(LogDR, Log, TEXT("[S2Turnstile] %s: 현재 자세를 닫힘 원점으로 다시 잡았습니다 (날개 %d개)."),
		*GetName(), Leaves.Num());
}
