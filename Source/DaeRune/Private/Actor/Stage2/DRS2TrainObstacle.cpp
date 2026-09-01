// Copyright DaeRune

#include "Actor/Stage2/DRS2TrainObstacle.h"

#include "Actor/Stage2/DRS2TrainTrack.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"

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

void ADRS2TrainObstacle::TryResolveStopDistance()
{
	// StopDistance 미입력 시 위치를 스플라인에 투영해 자동 계산한다.
	// 수동 입력 실수(오름차순 위반 등)를 줄이기 위해 자동 계산을 권장한다.
	if (StopDistance >= 0.f || !Track) return;

	const float Projected = Track->ProjectWorldLocationToDistance(GetActorLocation());
	StopDistance = FMath::Max(0.f, Projected - StopMargin);

	UE_LOG(LogDR, Log, TEXT("[S2Obstacle] %s: StopDistance 자동 계산 = %.0f (투영 %.0f - 여유 %.0f)"),
		*GetName(), StopDistance, Projected, StopMargin);
}

float ADRS2TrainObstacle::GetStopDistance()
{
	// ★지연 평가. 페이즈가 SetTrack() 으로 선로를 주입하는 시점이 BeginPlay 보다 늦기 때문이다.
	TryResolveStopDistance();

	if (StopDistance < 0.f)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2Obstacle] %s: StopDistance 를 정할 수 없습니다 (미입력 + Track 미배선). ")
			TEXT("Director 의 Track 배선 또는 이 장애물의 StopDistance 직접 입력이 필요합니다."), *GetName());

		return 0.f;
	}

	return StopDistance;
}

FTransform ADRS2TrainObstacle::GetBossSpawnTransform() const
{
	return BossSpawnPoint ? BossSpawnPoint->GetComponentTransform() : GetActorTransform();
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
