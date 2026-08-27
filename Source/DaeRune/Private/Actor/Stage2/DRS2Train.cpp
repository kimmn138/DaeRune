// Copyright DaeRune

#include "Actor/Stage2/DRS2Train.h"

#include "Actor/Stage2/DRS2TrainTrack.h"
#include "Actor/Stage2/DRS2TrainCar.h"
#include "Character/DRCharacter.h"
#include "Game/DRGameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"

ADRS2Train::ADRS2Train()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;

	// 이동은 로컬 시뮬로 처리한다. 이동 복제를 켜면 소스가 이중이 되어 떨림이 생긴다.
	SetReplicateMovement(false);

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
}

void ADRS2Train::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2Train, Movement);
}

void ADRS2Train::BeginPlay()
{
	Super::BeginPlay();

	InitializeCars();

	CurrentDistance = StartDistanceOnTrack;
	if (Track)
	{
		ApplyTransformAtDistance(CurrentDistance);
	}
	else if (HasAuthority())
	{
		UE_LOG(LogDR, Error, TEXT("[S2Train] Track 이 배선되지 않았습니다. 열차가 이동하지 않습니다."));
	}

	if (HasAuthority())
	{
		Movement.State = ES2TrainState::WaitingForBoarding;
		Movement.StartDistance = CurrentDistance;
		Movement.TargetDistance = CurrentDistance;
	}

	OnTrainStateChanged(Movement.State);
}

void ADRS2Train::InitializeCars()
{
	// 배열에서 무효 항목을 제거하고 순서대로 칸 번호를 부여한다.
	// ★배열 순서 = 칸 번호이므로 수동 인덱스 입력이 필요 없다(입력 실수 원천 차단).
	Cars.RemoveAll([](const TObjectPtr<ADRS2TrainCar>& Car) { return !IsValid(Car); });

	for (int32 Index = 0; Index < Cars.Num(); ++Index)
	{
		Cars[Index]->SetCarIndex(Index);
		Cars[Index]->SetOwningTrain(this);
	}

	if (HasAuthority())
	{
		if (Cars.Num() == 0)
		{
			UE_LOG(LogDR, Error, TEXT("[S2Train] 칸이 하나도 배선되지 않았습니다. Cars 배열을 채우세요."));
		}
		else
		{
			// 뒤 칸들이 선로 시작점(거리 0)에 겹치지 않으려면 시작 거리가 충분해야 한다
			const float RequiredStart = CarSpacing * static_cast<float>(Cars.Num() - 1);
			if (StartDistanceOnTrack < RequiredStart)
			{
				UE_LOG(LogDR, Warning,
					TEXT("[S2Train] StartDistanceOnTrack(%.0f) < 필요 거리(%.0f). 출발 전 뒤 칸들이 선로 시작점에 겹칩니다."),
					StartDistanceOnTrack, RequiredStart);
			}
		}
	}
}

// ================= 서버 API =================

void ADRS2Train::DepartTo(float InTargetDistance, float InSpeed, int32 InTargetIndex)
{
	if (!HasAuthority() || !Track) return;

	PendingTargetIndex = InTargetIndex;

	Movement.State = ES2TrainState::Moving;
	Movement.StartDistance = CurrentDistance;
	Movement.TargetDistance = FMath::Clamp(InTargetDistance, 0.f, Track->GetTrackLength());
	Movement.Speed = FMath::Max(1.f, InSpeed);

	if (const ADRGameStateBase* DRGameState = GetWorld()->GetGameState<ADRGameStateBase>())
	{
		Movement.StartServerTime = DRGameState->GetServerWorldTimeSeconds();
	}

	OnRep_Movement();

	UE_LOG(LogDR, Log, TEXT("[S2Train] 출발: %.0f -> %.0f (속도 %.0f)"),
		Movement.StartDistance, Movement.TargetDistance, Movement.Speed);
}

void ADRS2Train::SetWaitingForBoarding()
{
	if (!HasAuthority() || Movement.State == ES2TrainState::WaitingForBoarding) return;

	Movement.State = ES2TrainState::WaitingForBoarding;
	OnRep_Movement();
}

void ADRS2Train::SetArrived()
{
	if (!HasAuthority() || Movement.State == ES2TrainState::Arrived) return;

	Movement.State = ES2TrainState::Arrived;
	OnRep_Movement();
}

// ================= 이동 (로컬 시뮬) =================

void ADRS2Train::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Movement.State != ES2TrainState::Moving || !Track) return;

	const ADRGameStateBase* DRGameState = GetWorld() ? GetWorld()->GetGameState<ADRGameStateBase>() : nullptr;
	if (!DRGameState) return;

	const float Now = DRGameState->GetServerWorldTimeSeconds();
	const float Elapsed = FMath::Max(0.f, Now - Movement.StartServerTime);

	// 서버/클라가 같은 식으로 계산하므로 별도 위치 복제가 필요 없다
	const float Distance = FMath::Min(
		Movement.TargetDistance,
		Movement.StartDistance + Movement.Speed * Elapsed);

	CurrentDistance = Distance;
	ApplyTransformAtDistance(Distance);

	if (HasAuthority() && Distance >= Movement.TargetDistance - KINDA_SMALL_NUMBER)
	{
		ArriveAtTarget();
	}
}

void ADRS2Train::ApplyTransformAtDistance(float HeadDistance)
{
	if (!Track) return;

	// ★칸마다 자기 거리를 계산해 개별 배치한다.
	//   칸 i 거리 = HeadDistance - CarSpacing * i
	//   덕분에 선두 칸이 코너에 먼저 진입해 꺾이고 뒤 칸은 나중에 꺾인다.
	//   (칸들을 한 액터의 컴포넌트로 붙이면 강체로 함께 회전해 이 동작이 나오지 않는다.)
	for (const TObjectPtr<ADRS2TrainCar>& Car : Cars)
	{
		if (Car)
		{
			Car->UpdateFromTrack(Track, HeadDistance, CarSpacing);
		}
	}

	// 열차 액터 자신은 선두 위치로만 옮긴다 (디버그/사운드 부착 기준점).
	// 칸들은 attach 되어 있지 않으므로 이 이동이 칸에 영향을 주지 않는다.
	SetActorTransform(Track->GetTransformAtDistance(HeadDistance));
}

void ADRS2Train::ArriveAtTarget()
{
	const int32 ReachedIndex = PendingTargetIndex;
	PendingTargetIndex = INDEX_NONE;

	// 목표가 장애물이면 정지, 종점이면 도착
	Movement.State = ES2TrainState::StoppedAtObstacle;
	OnRep_Movement();

	if (ReachedIndex >= 0)
	{
		OnStoppedAtTarget.Broadcast(ReachedIndex);
	}
	else
	{
		// 종점 (인덱스 -1 로 출발한 경우)
		SetArrived();
		OnArrived.Broadcast();
	}
}

void ADRS2Train::OnRep_Movement()
{
	// 정지/대기 상태로 바뀌면 위치를 목표에 스냅해 서버-클라 편차를 없앤다
	if (Movement.State != ES2TrainState::Moving)
	{
		CurrentDistance = Movement.TargetDistance;
		ApplyTransformAtDistance(CurrentDistance);
	}

	OnTrainStateChanged(Movement.State);
}

// ================= 탑승 질의 =================

bool ADRS2Train::IsAcceptingBoarding() const
{
	// 정지 중(대기/장애물/도착)에만 탑승을 받는다
	return Movement.State != ES2TrainState::Moving;
}

bool ADRS2Train::CanDeboardNow() const
{
	// 하차는 정지 중에만 (§14.6.5). 이동 중 하차를 막아 "움직이는 발판 위 캐릭터" 문제를 회피한다.
	return Movement.State == ES2TrainState::StoppedAtObstacle
		|| Movement.State == ES2TrainState::Arrived
		|| Movement.State == ES2TrainState::WaitingForBoarding;
}

bool ADRS2Train::AreAllAlivePlayersSeated() const
{
	const ADRGameStateBase* DRGameState = GetWorld() ? GetWorld()->GetGameState<ADRGameStateBase>() : nullptr;
	if (!DRGameState) return false;

	const TArray<ADRCharacter*> AlivePlayers = DRGameState->GetAlivePlayers();
	if (AlivePlayers.Num() == 0) return false;

	// 빈 칸은 허용한다. 조건은 "생존자 전원이 어느 좌석엔가 앉아 있는지"다.
	for (ADRCharacter* Character : AlivePlayers)
	{
		bool bFound = false;
		for (const TObjectPtr<ADRS2TrainCar>& Car : Cars)
		{
			if (Car && Car->GetRider() == Character)
			{
				bFound = true;
				break;
			}
		}

		if (!bFound) return false;
	}

	return true;
}

void ADRS2Train::NotifySeatOccupancyChanged()
{
	if (!HasAuthority()) return;

	int32 SeatedCount = 0;
	for (const TObjectPtr<ADRS2TrainCar>& Car : Cars)
	{
		if (Car && Car->IsOccupied())
		{
			++SeatedCount;
		}
	}

	int32 AliveTotal = 0;
	if (const ADRGameStateBase* DRGameState = GetWorld()->GetGameState<ADRGameStateBase>())
	{
		AliveTotal = DRGameState->GetAlivePlayers().Num();
	}

	OnBoardingChanged.Broadcast(SeatedCount, AliveTotal);
}

void ADRS2Train::DeboardAll()
{
	if (!HasAuthority()) return;

	for (const TObjectPtr<ADRS2TrainCar>& Car : Cars)
	{
		if (Car && Car->IsOccupied())
		{
			Car->Deboard();
		}
	}
}
