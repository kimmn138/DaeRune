// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineMeshComponent.h"
#include "DRS2TrainTrack.generated.h"

class USplineComponent;

/**
 * 열차 선로 (Plan6 §4.9.1)
 *
 * ★경로를 정하는 것은 **스플라인**이다. 열차는 오직 스플라인만 보고 달린다.
 *   선로 메시는 시각일 뿐이며, 메시를 배치한다고 경로가 생기지 않는다.
 *
 * 선로 메시를 만드는 방법 두 가지:
 *   ① 반복 가능한 조각 메시가 있으면 TrackSegmentMesh 를 지정한다.
 *      → 스플라인을 따라 SplineMeshComponent 가 자동 생성되어 **메시가 스플라인과 절대 어긋나지 않는다**. (권장)
 *   ② ㄷ자 전체가 한 덩어리인 메시라면 TrackSegmentMesh 를 비우고 BP 에 StaticMeshComponent 로 직접 넣는다.
 *      → 이 경우 **스플라인을 메시 경로에 맞춰 손으로 편집**해야 한다.
 *
 * 확정 사양의 선로는 ㄷ자 형태에 코너 곡선이 약간 포함된다(§14.6.2).
 * GetTransformAtDistanceAlongSpline 이 회전까지 돌려주므로 코너에서 열차가 자연스럽게 돈다.
 */
UCLASS()
class DAERUNE_API ADRS2TrainTrack : public AActor
{
	GENERATED_BODY()

public:
	ADRS2TrainTrack();

	virtual void OnConstruction(const FTransform& Transform) override;

	USplineComponent* GetSpline() const { return Spline; }

	// 선로 전체 길이 (종점 거리로 사용)
	UFUNCTION(BlueprintCallable, Category = "S2|Track")
	float GetTrackLength() const;

	// 주어진 거리의 월드 트랜스폼
	UFUNCTION(BlueprintCallable, Category = "S2|Track")
	FTransform GetTransformAtDistance(float Distance) const;

	// 월드 위치를 스플라인 거리로 투영 (장애물의 StopDistance 자동 계산에 사용)
	UFUNCTION(BlueprintCallable, Category = "S2|Track")
	float ProjectWorldLocationToDistance(const FVector& WorldLocation) const;

	// 스플라인을 따라 선로 메시를 다시 생성한다 (에디터에서 수동 갱신용)
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "S2|Track")
	void RebuildTrackMesh();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Track")
	TObjectPtr<USplineComponent> Spline;

	// ========== 선로 메시 자동 생성 (선택) ==========

	// 반복 배치할 선로 조각 메시. **비워두면 아무것도 생성하지 않는다**
	// (ㄷ자 전체가 한 덩어리인 메시를 BP 에 직접 넣는 방식과 병행 가능).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Track|Mesh")
	TObjectPtr<UStaticMesh> TrackSegmentMesh;

	// 조각 하나의 진행 방향 길이(uu). 0 이면 메시 바운드에서 자동 계산한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Track|Mesh", meta = (ClampMin = "0.0"))
	float SegmentLength = 0.f;

	// 조각 메시의 진행 방향 축 (대부분 X)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Track|Mesh")
	TEnumAsByte<ESplineMeshAxis::Type> SegmentForwardAxis = ESplineMeshAxis::X;

	// 생성된 선로 메시에 콜리전을 줄지 (플레이어가 선로 위를 걸어야 하면 true)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Track|Mesh")
	bool bSegmentCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Track|Mesh",
		meta = (EditCondition = "bSegmentCollision"))
	FName SegmentCollisionProfile = TEXT("BlockAll");

private:
	// 자동 생성된 선로 메시 조각들
	UPROPERTY()
	TArray<TObjectPtr<USplineMeshComponent>> SegmentMeshComponents;

	void ClearSegmentMeshes();
};
