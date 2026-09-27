// Copyright DaeRune

#include "Actor/Stage2/DRS2TrainTrack.h"

#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "DaeRune/DRLogChannels.h"

namespace
{
	const TCHAR* SplinePointTypeToString(ESplinePointType::Type Type)
	{
		switch (Type)
		{
		case ESplinePointType::Linear:				return TEXT("Linear");
		case ESplinePointType::Curve:				return TEXT("Curve");
		case ESplinePointType::Constant:			return TEXT("Constant");
		case ESplinePointType::CurveClamped:		return TEXT("CurveClamped");
		case ESplinePointType::CurveCustomTangent:	return TEXT("CurveCustomTangent");
		default:									return TEXT("?");
		}
	}
}

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

void ADRS2TrainTrack::LogTrackInfo()
{
	if (!Spline)
	{
		UE_LOG(LogDR, Error, TEXT("[S2Track] %s: 스플라인이 없습니다."), *GetName());
		return;
	}

	const int32 PointCount = Spline->GetNumberOfSplinePoints();

	UE_LOG(LogDR, Warning, TEXT("[S2Track] %s: 전체 길이 %.0f uu / 포인트 %d개 / ClosedLoop=%s"),
		*GetName(), Spline->GetSplineLength(), PointCount,
		Spline->IsClosedLoop() ? TEXT("ON(★꺼야 한다)") : TEXT("off"));

	UE_LOG(LogDR, Warning, TEXT("[S2Track]   액터 트랜스폼: 위치 %s / 회전 %s / 스케일 %s"),
		*GetActorLocation().ToCompactString(),
		*GetActorRotation().ToCompactString(),
		*GetActorScale3D().ToCompactString());

	for (int32 Index = 0; Index < PointCount; ++Index)
	{
		const float Distance = Spline->GetDistanceAlongSplineAtSplinePoint(Index);

		UE_LOG(LogDR, Warning, TEXT("[S2Track]   [%d] 월드 위치 %s"), Index,
			*Spline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World).ToCompactString());

		// ★포인트 타입은 **그 포인트에서 다음 포인트로 가는 구간**의 보간 방식이다.
		//   따라서 마지막 포인트의 타입은 나가는 구간이 없어 의미가 없다.
		const bool bHasOutgoingSegment = (Index < PointCount - 1);

		UE_LOG(LogDR, Warning, TEXT("[S2Track]   [%d] 거리 %8.0f  타입 %-18s %s"),
			Index, Distance,
			SplinePointTypeToString(Spline->GetSplinePointType(Index)),
			bHasOutgoingSegment ? TEXT("") : TEXT("(마지막 - 타입 무의미)"));
	}
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
