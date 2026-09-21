// Copyright DaeRune

#include "Actor/Stage2/DRS2TrainObstacle.h"

#include "Actor/Stage2/DRS2TrainTrack.h"
#include "Components/ArrowComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"

namespace
{
	// 장애물이 선로에서 이만큼 벗어나 있으면 배치 실수로 본다.
	// ★스플라인 투영은 아무리 멀어도 "가장 가까운 점"을 돌려주기 때문에,
	//   원점에 놓인 채 방치된 액터가 조용히 거리 0(첫 포인트)으로 투영되는 사고가 난다.
	constexpr float MaxLateralDistanceFromTrack = 3000.f;
}

ADRS2TrainObstacle::ADRS2TrainObstacle()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ObstacleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ObstacleMesh"));
	ObstacleMesh->SetupAttachment(SceneRoot);

	BossSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("BossSpawnPoint"));
	BossSpawnPoint->SetupAttachment(SceneRoot);

	// ★열차 정지 지점. 인스턴스마다 선로 위로 옮겨 쓴다 (클래스 주석의 배치 규약 참조).
	StopPoint = CreateDefaultSubobject<USceneComponent>(TEXT("StopPoint"));
	StopPoint->SetupAttachment(SceneRoot);

#if WITH_EDITORONLY_DATA
	StopPointArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("StopPointArrow"));
	if (StopPointArrow)
	{
		StopPointArrow->SetupAttachment(StopPoint);
		StopPointArrow->ArrowColor = FColor(64, 192, 255);
		StopPointArrow->ArrowSize = 1.5f;
	}
#endif
}

void ADRS2TrainObstacle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2TrainObstacle, bBroken);
}

void ADRS2TrainObstacle::BeginPlay()
{
	Super::BeginPlay();

	// 에디터에서 Track 을 직접 배선했다면 여기서 미리 계산해 둔다.
	// 배선하지 않았다면 페이즈가 SetTrack() 을 호출한 뒤 GetStopDistance() 에서 계산된다.
	TryResolveStopDistance();
}

FVector ADRS2TrainObstacle::GetTrackReferenceLocation() const
{
	// ★선로 투영의 기준점은 액터 루트도, 메시도 아닌 **StopPoint** 다.
	//   장애물 메시는 피벗이 월드 원점에 구워진 맵 에셋이라 액터를 (0,0,0) 에서 움직일 수 없고,
	//   그래서 액터·메시 위치는 선로와 아무 관계가 없다. 정지 지점만 따로 찍어 쓴다.
	return StopPoint ? StopPoint->GetComponentLocation() : GetActorLocation();
}

void ADRS2TrainObstacle::TryResolveStopDistance()
{
	if (bStopDistanceResolved) return;

	// ★"미입력"을 0 이하로 판정한다. 센티넬(-1)에 의존하지 않는 이유:
	//   BP CDO 나 레벨 인스턴스에 0 이 한 번 직렬화되면 C++ 기본값을 -1 로 바꿔도 0 이 이기고,
	//   그러면 자동 계산이 통째로 건너뛰어져 열차가 거리 0 으로 출발하려다 멈춘다.
	//   선로 시작점(거리 0)은 애초에 유효한 정지 지점이 아니므로 0 을 미입력으로 봐도 잃는 것이 없다.
	if (StopDistance > 0.f)
	{
		bStopDistanceResolved = true;
		return;
	}

	// 위치를 스플라인에 투영해 자동 계산한다.
	// 수동 입력 실수(오름차순 위반 등)를 줄이기 위해 자동 계산을 권장한다.
	if (!Track) return;

	const FVector MyLocation = GetTrackReferenceLocation();

	// ★선로에서 너무 멀면 투영값을 신뢰하지 않는다. 미해결로 남겨 GetStopDistance 가 에러를 내게 하고,
	//   열차는 ADRS2Train::DepartTo 의 가드에 걸려 출발을 거부한다 (잘못된 거리로 달리는 것보다 낫다).
	if (const USplineComponent* Spline = Track->GetSpline())
	{
		const FVector Nearest = Spline->FindLocationClosestToWorldLocation(MyLocation, ESplineCoordinateSpace::World);
		const float Lateral = FVector::Dist(MyLocation, Nearest);

		if (Lateral > MaxLateralDistanceFromTrack)
		{
			UE_LOG(LogDR, Error,
				TEXT("[S2Obstacle] %s: StopPoint 가 선로에서 %.0f uu 떨어져 있습니다 ")
				TEXT("(StopPoint %s / 최근접 선로 지점 %s). ")
				TEXT("이 액터를 선택해 StopPoint 컴포넌트를 선로 위 장애물 앞으로 옮기세요."),
				*GetName(), Lateral, *MyLocation.ToCompactString(), *Nearest.ToCompactString());

			return;
		}
	}

	// ★StopPoint 가 곧 정지 지점이다. 여유 거리를 더하지 않는다
	//   (원하는 간격은 포인트를 놓는 위치로 표현한다).
	StopDistance = Track->ProjectWorldLocationToDistance(MyLocation);
	bStopDistanceResolved = true;
}

float ADRS2TrainObstacle::GetStopDistance()
{
	// ★지연 평가. 페이즈가 SetTrack() 으로 선로를 주입하는 시점이 BeginPlay 보다 늦기 때문이다.
	TryResolveStopDistance();

	if (!bStopDistanceResolved)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2Obstacle] %s: StopDistance 미해결 (Track 미배선이거나 선로에서 너무 멂 - 위 로그 참조). ")
			TEXT("장애물을 선로 위로 옮기거나 StopDistance 를 직접 입력하세요."), *GetName());

		return 0.f;
	}

	return StopDistance;
}

FTransform ADRS2TrainObstacle::GetBossSpawnTransform() const
{
	FTransform SpawnTransform = BossSpawnPoint ? BossSpawnPoint->GetComponentTransform() : GetActorTransform();

	// ★스폰 지점은 "어디에 세울지"만 정한다. 보스 크기는 BP_S2MoleBoss 가 정한다.
	//   컴포넌트 스케일을 그대로 흘려보내면 스폰 지점을 키우는 순간 보스까지 커진다.
	SpawnTransform.SetScale3D(FVector::OneVector);

	// ★BossSpawnPoint 도 액터 루트에 붙어 있어 StopPoint 와 똑같은 함정이 있다.
	//   옮기지 않으면 정지 거리는 맞는데 보스만 월드 원점에 튀어나온다.
	//   조용히 넘어가면 찾기 어려운 버그라 로그로 못 박는다.
	const FVector StopLocation = GetTrackReferenceLocation();
	const float Gap = FVector::Dist(SpawnTransform.GetLocation(), StopLocation);

	if (Gap > MaxLateralDistanceFromTrack)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2Obstacle] %s: 보스 스폰 지점이 정지 지점에서 %.0f uu 떨어져 있습니다 ")
			TEXT("(스폰 %s / StopPoint %s). BossSpawnPoint 컴포넌트를 장애물 옆으로 옮기세요."),
			*GetName(), Gap,
			*SpawnTransform.GetLocation().ToCompactString(), *StopLocation.ToCompactString());
	}

	return SpawnTransform;
}

void ADRS2TrainObstacle::BreakByBoss()
{
	if (!HasAuthority() || bBroken) return;

	bBroken = true;
	OnRep_bBroken();
}

void ADRS2TrainObstacle::OnRep_bBroken()
{
	if (!bBroken) return;

	// 부서진 뒤에는 시각/충돌 모두 제거한다. 전방 차단은 ADRS2Barrier 가 담당한다.
	if (ObstacleMesh)
	{
		ObstacleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ObstacleMesh->SetVisibility(false);
	}

	OnBrokenVisual();
}
