// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/Stage2/DRS2PassageBlocker.h"
#include "DRS2TurnstileDoor.generated.h"

/**
 * 지하철 개찰구식 회전문 (Plan6 §4.11-A)
 *
 * 날개 메시들이 두 그룹으로 나뉘어 **서로 반대 방향으로 회전**하며 열린다.
 * 닫히면 각 날개가 배치 당시의 원점 회전으로 정확히 되돌아간다.
 *
 *   D2 (방1 <-> 방3) : 열림 시작, 전원 방3 입장 시 닫힘
 *   D4 (방3 <-> 방5) : 닫힘 시작, 두더지 클리어 시 열림 -> 전원 방5 입장 시 재차 닫힘 (왕복)
 *
 * 날개 구성은 **컴포넌트 태그**로 한다. BP 에서 날개 메시를 추가하고
 * GroupATag / GroupBTag 를 달면 BeginPlay 에서 자동 수집한다.
 * (컴포넌트 참조 배열은 BP 디테일 패널에서 배선할 수 없어 태그 방식을 쓴다 — CCTV·열차 칸과 동일 관례)
 *
 * ★날개 메시의 **피벗이 회전축(힌지)에 있어야** 한다. 피벗이 메시 중앙이면 제자리에서 빙글 돈다.
 *   피벗을 옮길 수 없으면 BP 에서 힌지 위치에 SceneComponent 를 두고 그 자식으로 메시를 붙인 뒤,
 *   **SceneComponent 쪽에 태그를 단다**(회전은 태그가 달린 컴포넌트에 적용된다).
 */
UCLASS()
class DAERUNE_API ADRS2TurnstileDoor : public ADRS2PassageBlocker
{
	GENERATED_BODY()

public:
	ADRS2TurnstileDoor();

	// 에디터에서 열림 각도를 확인하기 위한 미리보기.
	// ★확인이 끝나면 반드시 PreviewClosedPose 로 되돌린 뒤 저장한다.
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "S2|Turnstile")
	void PreviewOpenPose();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "S2|Turnstile")
	void PreviewClosedPose();

	// 날개를 다시 배치했을 때 현재 자세를 닫힘 원점으로 재설정한다.
	// ★반드시 닫힌 자세에서 누른다.
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "S2|Turnstile")
	void ResetPreviewOrigin();

protected:
	virtual void InitializePose() override;
	virtual void ApplyPose(float Alpha) override;

	// ===== 날개 수집 =====

	// 한쪽으로 회전하는 날개 그룹의 컴포넌트 태그 (보통 3개)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Turnstile")
	FName GroupATag = TEXT("TurnstileA");

	// 반대쪽으로 회전하는 날개 그룹의 컴포넌트 태그 (보통 3개)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Turnstile")
	FName GroupBTag = TEXT("TurnstileB");

	// ===== 회전량 =====

	// 그룹 A 가 완전히 열렸을 때의 회전 변화량.
	// ★**문(액터) 기준 축**이다. 날개 메시를 어떻게 돌려 배치했든 이 축으로 돈다.
	//   개찰구 날개는 보통 Yaw 회전이다. 예: (0, 90, 0)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Turnstile")
	FRotator OpenRotationA = FRotator(0.f, 90.f, 0.f);

	// true 면 그룹 B 는 A 의 정확히 반대 방향으로 회전한다 (부호 반전).
	// 회전이 문 기준 축이므로 배치 방향과 무관하게 항상 반대로 돈다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Turnstile")
	bool bMirrorGroupB = true;

	// bMirrorGroupB 가 false 일 때 그룹 B 의 회전량을 직접 지정한다.
	// 두 그룹을 같은 방향으로 돌리고 싶을 때 A 와 같은 값을 넣는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Turnstile",
		meta = (EditCondition = "!bMirrorGroupB"))
	FRotator OpenRotationB = FRotator(0.f, -90.f, 0.f);

private:
	// 날개 하나의 회전 정보. 원점(배치 당시 상대 회전)과 열림 목표를 미리 계산해 둔다.
	struct FTurnstileLeaf
	{
		TWeakObjectPtr<USceneComponent> Component;
		FQuat ClosedQuat = FQuat::Identity;
		FQuat OpenQuat = FQuat::Identity;
	};

	void CollectLeaves();
	void CollectGroup(FName Tag, const FRotator& OpenRotation);

	// 아직 수집하지 않았을 때만 수집한다 (열린 자세를 원점으로 오인하는 것을 막는다)
	void EnsureLeavesCollected();

	TArray<FTurnstileLeaf> Leaves;
};
