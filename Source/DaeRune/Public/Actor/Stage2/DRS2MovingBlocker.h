// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/Stage2/DRS2PassageBlocker.h"
#include "DRS2MovingBlocker.generated.h"

class UStaticMeshComponent;

/**
 * 상하 이동 구조물 (Plan6 §4.11)
 *
 * 문이 열리고 닫히는 방식이 아니라 구조물이 올라오거나 내려가서 통로를 막고/여는 연출 액터.
 *
 *   D0 (시작지점 -> 방1) : 열림 시작, 전원 방1 입장 시 상승 봉쇄
 *   D5 (방5 -> 방6)      : 막힘 시작, 방5 3웨이브 클리어 시 하강 개방
 *
 * ※ D2(방1<->방3), D4(방3<->방5)는 개찰구식 회전문(ADRS2TurnstileDoor)을 쓴다.
 *
 * 상태 복제·콜리전 토글·끼임 방어는 베이스(ADRS2PassageBlocker)가 처리하고,
 * 이 클래스는 "두 오프셋 사이를 이동하는" 포즈만 담당한다.
 */
UCLASS()
class DAERUNE_API ADRS2MovingBlocker : public ADRS2PassageBlocker
{
	GENERATED_BODY()

public:
	ADRS2MovingBlocker();

protected:
	virtual void ApplyPose(float Alpha) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Blocker")
	TObjectPtr<USceneComponent> MovingRoot;

	// 실제로 움직이는 구조물 메시. 여러 개가 필요하면 BP에서 MovingRoot 자식으로 추가한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Blocker")
	TObjectPtr<UStaticMeshComponent> BlockerMesh;

	// "막힘" 포즈의 로컬 오프셋 (상승형은 +Z, 하강형은 0)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Blocker")
	FVector BlockedOffset = FVector(0.f, 0.f, 300.f);

	// "열림" 포즈의 로컬 오프셋 (상승형은 0, 하강형은 -Z)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Blocker")
	FVector OpenOffset = FVector::ZeroVector;
};
