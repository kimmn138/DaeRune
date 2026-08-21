// Copyright DaeRune

#include "Actor/Stage2/DRS2TrainTrack.h"

#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"

ADRS2TrainTrack::ADRS2TrainTrack()
{
	PrimaryActorTick.bCanEverTick = false;

	// 순수 데이터 액터. 런타임에 바뀌는 상태가 없어 복제하지 않는다.
	bReplicates = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	SetRootComponent(Spline);
	Spline->SetClosedLoop(false);
}

void ADRS2TrainTrack::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 스플라인을 편집하면 에디터에서 즉시 선로 메시가 갱신된다
	RebuildTrackMesh();
}

float ADRS2TrainTrack::GetTrackLength() const
{
	return Spline ? Spline->GetSplineLength() : 0.f;
}

FTransform ADRS2TrainTrack::GetTransformAtDistance(float Distance) const
{
	if (!Spline) return GetActorTransform();

	const float ClampedDistance = FMath::Clamp(Distance, 0.f, Spline->GetSplineLength());
	return Spline->GetTransformAtDistanceAlongSpline(ClampedDistance, ESplineCoordinateSpace::World);
}

float ADRS2TrainTrack::ProjectWorldLocationToDistance(const FVector& WorldLocation) const
{
	if (!Spline) return 0.f;

	const float InputKey = Spline->FindInputKeyClosestToWorldLocation(WorldLocation);
	return Spline->GetDistanceAlongSplineAtSplineInputKey(InputKey);
}

// ================= 선로 메시 자동 생성 =================

void ADRS2TrainTrack::ClearSegmentMeshes()
{
	for (USplineMeshComponent* Segment : SegmentMeshComponents)
	{
		if (IsValid(Segment))
		{
			Segment->DestroyComponent();
		}
	}

	SegmentMeshComponents.Reset();
}

void ADRS2TrainTrack::RebuildTrackMesh()
{
	ClearSegmentMeshes();

	// 조각 메시가 없으면 아무것도 하지 않는다.
	// (ㄷ자 전체가 한 덩어리인 메시를 BP 에 직접 넣는 방식과 병행하기 위함)
	if (!TrackSegmentMesh || !Spline) return;

	const float TotalLength = Spline->GetSplineLength();
	if (TotalLength <= KINDA_SMALL_NUMBER) return;

	// 조각 길이: 지정값이 없으면 메시 바운드의 진행 축 크기로 자동 계산
	float PieceLength = SegmentLength;
	if (PieceLength <= 0.f)
	{
		const FVector Extent = TrackSegmentMesh->GetBounds().BoxExtent;
		switch (SegmentForwardAxis)
		{
		case ESplineMeshAxis::Y: PieceLength = Extent.Y * 2.f; break;
		case ESplineMeshAxis::Z: PieceLength = Extent.Z * 2.f; break;
		default:                 PieceLength = Extent.X * 2.f; break;
		}
	}

	if (PieceLength <= 1.f) return;

	// 남는 자투리가 생기지 않도록 전체 길이를 균등 분할한다
	const int32 SegmentCount = FMath::Max(1, FMath::RoundToInt(TotalLength / PieceLength));
	const float ActualLength = TotalLength / static_cast<float>(SegmentCount);

	const EComponentMobility::Type TargetMobility =
		Spline ? Spline->Mobility.GetValue() : EComponentMobility::Movable;

	SegmentMeshComponents.Reserve(SegmentCount);

	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const float StartDistance = ActualLength * static_cast<float>(Index);
		const float EndDistance = ActualLength * static_cast<float>(Index + 1);

		USplineMeshComponent* Segment = NewObject<USplineMeshComponent>(this);
		if (!Segment) continue;

		Segment->SetMobility(TargetMobility);
		Segment->SetupAttachment(Spline);
		Segment->RegisterComponent();

		Segment->SetStaticMesh(TrackSegmentMesh);
		Segment->SetForwardAxis(SegmentForwardAxis, false);

		const FVector StartPos = Spline->GetLocationAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local);
		const FVector EndPos = Spline->GetLocationAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local);

		// 스플라인 접선의 크기는 전체 구간 길이 기준이라 조각에 그대로 쓰면 과도하게 휜다.
		// 방향만 취하고 조각 길이로 다시 스케일한다.
		const FVector StartTangent =
			Spline->GetTangentAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::Local).GetSafeNormal() * ActualLength;
		const FVector EndTangent =
			Spline->GetTangentAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::Local).GetSafeNormal() * ActualLength;

		Segment->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true);

		if (bSegmentCollision)
		{
			Segment->SetCollisionProfileName(SegmentCollisionProfile);
			Segment->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		else
		{
			Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		SegmentMeshComponents.Add(Segment);
	}
}
