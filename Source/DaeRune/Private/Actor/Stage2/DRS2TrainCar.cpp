// Copyright DaeRune

#include "Actor/Stage2/DRS2TrainCar.h"

#include "Actor/Stage2/DRS2Train.h"
#include "Actor/Stage2/DRS2TrainTrack.h"
#include "Character/DRCharacter.h"
#include "Components/WidgetComponent.h"
#include "Interaction/CombatInterface.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"

ADRS2TrainCar::ADRS2TrainCar()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	// 이동은 열차 Tick 이 서버·클라 양쪽에서 동일 계산으로 만든다.
	// 이동 복제를 켜면 소스가 이중이 되어 떨린다.
	SetReplicateMovement(false);

	CarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarMesh"));
	SetRootComponent(CarMesh);

	// 시점 라인트레이스(ECC_Visibility)에 걸려야 탑승 감지가 된다.
	CarMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CarMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	RiderAttachPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RiderAttachPoint"));
	RiderAttachPoint->SetupAttachment(CarMesh);

	ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(CarMesh);

	InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
	InteractionWidget->SetupAttachment(CarMesh);
	InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionWidget->SetVisibility(false);
}

void ADRS2TrainCar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2TrainCar, Rider);
}

void ADRS2TrainCar::BeginPlay()
{
	Super::BeginPlay();

	// 열차가 배선하지 않았다면 부모 액터에서 추론한다
	if (!OwningTrain)
	{
		OwningTrain = Cast<ADRS2Train>(GetAttachParentActor());
	}

	if (HasAuthority() && !OwningTrain)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2TrainCar] %s: 소유 열차를 찾지 못했습니다. 열차의 Cars 배열에 배선하세요."), *GetName());
	}
}

// ================= 선로 추종 =================

void ADRS2TrainCar::UpdateFromTrack(const ADRS2TrainTrack* Track, float HeadDistance, float CarSpacing)
{
	if (!Track) return;

	// ★칸마다 자기 거리를 갖는 것이 곡선 추종의 핵심이다.
	//   선두가 코너에 먼저 진입해 꺾이고, 뒤 칸은 나중에 같은 지점을 지나며 꺾인다.
	const float MyDistance = FMath::Max(0.f, HeadDistance - CarSpacing * static_cast<float>(CarIndex));

	const FTransform TrackTransform = Track->GetTransformAtDistance(MyDistance);

	FRotator Rotation = TrackTransform.Rotator();
	if (bLevelPitchAndRoll)
	{
		// 스플라인 포인트의 경사·뱅킹을 무시하고 수평 유지
		Rotation.Pitch = 0.f;
		Rotation.Roll = 0.f;
	}

	// 높이 보정은 칸의 위쪽 방향 기준으로 적용한다 (수평 유지 시 월드 Z 와 동일)
	const FVector Location = TrackTransform.GetLocation() + Rotation.RotateVector(FVector::UpVector) * HeightOffset;

	// ★스케일은 적용하지 않는다.
	//   GetTransformAtDistanceAlongSpline 은 스플라인 포인트의 스케일을 포함하므로,
	//   그대로 쓰면 스플라인 포인트 스케일이 1이 아닐 때 칸 크기가 변한다.
	SetActorLocationAndRotation(Location, Rotation);
}

// ================= 탑승 =================

bool ADRS2TrainCar::CanBeBoardedBy(const ADRCharacter* Candidate) const
{
	if (!IsValid(Candidate)) return false;

	// 이미 탑승자가 있으면 거부 (한 칸에 1명)
	if (Rider != nullptr) return false;

	// ADRRobotVacuumCharacter::CanBeMountedBy 검증 항목 미러
	if (ICombatInterface::Execute_IsDead(const_cast<ADRCharacter*>(Candidate))) return false;
	if (Candidate->IsCarryingPart()) return false;
	if (Candidate->IsMounted()) return false;
	if (Candidate->IsSeatedOnTrain()) return false;

	// 열차가 탑승을 받는 상태여야 한다 (주행 중에는 못 탄다)
	if (!OwningTrain || !OwningTrain->IsAcceptingBoarding()) return false;

	return true;
}

void ADRS2TrainCar::Board(ADRCharacter* Character)
{
	if (!HasAuthority() || !IsValid(Character)) return;
	if (Rider != nullptr) return;

	Rider = Character;

	// 캐릭터 측 상태 설정 (attach + 이동 잠금)
	Character->SetSeatedOn(this);

	// 탑승자 사망 시 칸 자동 해제
	if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Character))
	{
		CombatInterface->GetOnDeathDelegate().AddDynamic(this, &ADRS2TrainCar::HandleRiderDeath);
	}

	OnRep_Rider();

	if (OwningTrain)
	{
		OwningTrain->NotifySeatOccupancyChanged();
	}
}

void ADRS2TrainCar::Deboard()
{
	if (!HasAuthority() || !Rider) return;

	ADRCharacter* Leaving = Rider;

	if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Leaving))
	{
		CombatInterface->GetOnDeathDelegate().RemoveDynamic(this, &ADRS2TrainCar::HandleRiderDeath);
	}

	Rider = nullptr;

	// 캐릭터 측 상태 해제 (detach + 이동 복구)
	Leaving->SetSeatedOn(nullptr);

	// 하차 지점으로 내려놓는다
	if (ExitPoint)
	{
		Leaving->SetActorLocation(ExitPoint->GetComponentLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	}

	OnRep_Rider();

	if (OwningTrain)
	{
		OwningTrain->NotifySeatOccupancyChanged();
	}
}

void ADRS2TrainCar::HandleRiderDeath(AActor* /*DeadActor*/)
{
	if (!HasAuthority()) return;

	// 사망 시 칸을 비운다. 위치 이동은 하지 않는다(시체를 칸에서 떼기만 한다).
	if (ADRCharacter* Dead = Rider)
	{
		if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Dead))
		{
			CombatInterface->GetOnDeathDelegate().RemoveDynamic(this, &ADRS2TrainCar::HandleRiderDeath);
		}

		Rider = nullptr;
		Dead->SetSeatedOn(nullptr);
		OnRep_Rider();
	}

	if (OwningTrain)
	{
		OwningTrain->NotifySeatOccupancyChanged();
	}
}

void ADRS2TrainCar::OnRep_Rider()
{
	if (Rider)
	{
		InteractionWidget->SetVisibility(false);
	}

	OnRiderChanged(Rider);
}

void ADRS2TrainCar::SetInteractionUIVisible(bool bShow)
{
	// 로컬 코스메틱. 이미 탑승자가 있으면 프롬프트를 띄우지 않는다.
	InteractionWidget->SetVisibility(bShow && Rider == nullptr);
}
