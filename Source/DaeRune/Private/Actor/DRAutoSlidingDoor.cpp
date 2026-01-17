// Copyright DaeRune


#include "Actor/DRAutoSlidingDoor.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Character/DRCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

ADRAutoSlidingDoor::ADRAutoSlidingDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	// Root Component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	SetRootComponent(RootSceneComponent);

	// Left Door Mesh
	LeftDoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftDoorMesh"));
	LeftDoorMesh->SetupAttachment(RootSceneComponent);
	LeftDoorMesh->SetCollisionProfileName(TEXT("BlockAll"));

	// Right Door Mesh
	RightDoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightDoorMesh"));
	RightDoorMesh->SetupAttachment(RootSceneComponent);
	RightDoorMesh->SetCollisionProfileName(TEXT("BlockAll"));

	// Trigger Volume
	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->SetupAttachment(RootSceneComponent);
	TriggerVolume->SetCollisionProfileName(TEXT("Trigger"));
	TriggerVolume->SetGenerateOverlapEvents(true);
	TriggerVolume->SetBoxExtent(FVector(200.0f, 300.0f, 200.0f));

	// Audio Component
	DoorAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("DoorAudioComponent"));
	DoorAudioComponent->SetupAttachment(RootSceneComponent);
	DoorAudioComponent->bAutoActivate = false;
}

void ADRAutoSlidingDoor::BeginPlay()
{
	Super::BeginPlay();

	// 문 위치 계산
	CalculateDoorLocations();

	// 시작 상태 설정
	if (bStartOpen)
	{
		CurrentDoorState = EDoorState::Open;
		CurrentOpenRatio = 1.0f;
		TargetOpenRatio = 1.0f;

		// 즉시 열린 위치로 이동
		LeftDoorMesh->SetRelativeLocation(LeftDoorOpenLocation);
		RightDoorMesh->SetRelativeLocation(RightDoorOpenLocation);
	}
	else
	{
		CurrentDoorState = EDoorState::Closed;
		CurrentOpenRatio = 0.0f;
		TargetOpenRatio = 0.0f;
	}

	// 초기 위치 적용
	UpdateDoorPosition(0.0f);

	// Overlap 이벤트 바인딩
	if (HasAuthority())
	{
		TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &ADRAutoSlidingDoor::OnTriggerBeginOverlap);
		TriggerVolume->OnComponentEndOverlap.AddDynamic(this, &ADRAutoSlidingDoor::OnTriggerEndOverlap);
	}
}

void ADRAutoSlidingDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRAutoSlidingDoor, CurrentDoorState);
	DOREPLIFETIME(ADRAutoSlidingDoor, bIsLocked);
}

void ADRAutoSlidingDoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 문이 움직이는 중이면 위치 업데이트
	if (CurrentDoorState == EDoorState::Opening || CurrentDoorState == EDoorState::Closing)
	{
		UpdateDoorPosition(DeltaTime);
	}
}

void ADRAutoSlidingDoor::CalculateDoorLocations()
{
	// 현재 위치를 닫힌 위치로 저장
	LeftDoorClosedLocation = LeftDoorMesh->GetRelativeLocation();
	RightDoorClosedLocation = RightDoorMesh->GetRelativeLocation();

	// 열린 위치 계산
	LeftDoorOpenLocation = LeftDoorClosedLocation + FVector(DoorOpenOffset, 0.0f, 0.0f);
	RightDoorOpenLocation = RightDoorClosedLocation + FVector(-DoorOpenOffset, 0.0f, 0.0f);
}

void ADRAutoSlidingDoor::UpdateDoorPosition(float DeltaTime)
{
	// 목표 비율을 향해 부드럽게 이동
	if (!FMath::IsNearlyEqual(CurrentOpenRatio, TargetOpenRatio, 0.001f))
	{
		float InterpSpeed = DoorSpeed / DoorOpenOffset; // 속도를 비율 변화량으로 변환
		CurrentOpenRatio = FMath::FInterpTo(CurrentOpenRatio, TargetOpenRatio, DeltaTime, InterpSpeed);

		// 문 위치 적용
		FVector LeftNewLocation = FMath::Lerp(LeftDoorClosedLocation, LeftDoorOpenLocation, CurrentOpenRatio);
		FVector RightNewLocation = FMath::Lerp(RightDoorClosedLocation, RightDoorOpenLocation, CurrentOpenRatio);

		LeftDoorMesh->SetRelativeLocation(LeftNewLocation);
		RightDoorMesh->SetRelativeLocation(RightNewLocation);
	}
	else
	{
		// 이동 완료
		CurrentOpenRatio = TargetOpenRatio;

		if (CurrentDoorState == EDoorState::Opening)
		{
			CurrentDoorState = EDoorState::Open;
			OnDoorOpened.Broadcast();
		}
		else if (CurrentDoorState == EDoorState::Closing)
		{
			CurrentDoorState = EDoorState::Closed;
			OnDoorClosed.Broadcast();
		}
	}
}

void ADRAutoSlidingDoor::SetDoorState(EDoorState NewState)
{
	if (!HasAuthority()) return;
	if (bIsLocked && (NewState == EDoorState::Opening || NewState == EDoorState::Open)) return;

	EDoorState PreviousState = CurrentDoorState;
	CurrentDoorState = NewState;

	// 목표 비율 설정 및 사운드 재생
	switch (NewState)
	{
		case EDoorState::Opening:
			TargetOpenRatio = 1.0f;
			if (DoorOpenSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, DoorOpenSound, GetActorLocation());
			}
			break;

		case EDoorState::Closing:
			TargetOpenRatio = 0.0f;
			if (DoorCloseSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, DoorCloseSound, GetActorLocation());
			}
			break;

		case EDoorState::Open:
			TargetOpenRatio = 1.0f;
			CurrentOpenRatio = 1.0f;
			break;

		case EDoorState::Closed:
			TargetOpenRatio = 0.0f;
			CurrentOpenRatio = 0.0f;
			break;
	}

	// 클라이언트에도 즉시 적용
	OnRep_DoorState();
}

void ADRAutoSlidingDoor::OnRep_DoorState()
{
	// 클라이언트에서 상태 변경 시 호출됨
	switch (CurrentDoorState)
	{
		case EDoorState::Opening:
			TargetOpenRatio = 1.0f;
			if (DoorOpenSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, DoorOpenSound, GetActorLocation());
			}
			break;

		case EDoorState::Closing:
			TargetOpenRatio = 0.0f;
			if (DoorCloseSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, DoorCloseSound, GetActorLocation());
			}
			break;

		case EDoorState::Open:
			TargetOpenRatio = 1.0f;
			CurrentOpenRatio = 1.0f;
			UpdateDoorPosition(0.0f);
			break;

		case EDoorState::Closed:
			TargetOpenRatio = 0.0f;
			CurrentOpenRatio = 0.0f;
			UpdateDoorPosition(0.0f);
			break;
	}
}

void ADRAutoSlidingDoor::OpenDoor()
{
	if (!HasAuthority()) return;

	if (CurrentDoorState == EDoorState::Open || CurrentDoorState == EDoorState::Opening) return;
	if (bIsLocked) return;

	bCloseOnPlayerOverlap = false;

	SetDoorState(EDoorState::Opening);
}

void ADRAutoSlidingDoor::CloseDoor()
{
	if (!HasAuthority()) return;

	if (CurrentDoorState == EDoorState::Closed || CurrentDoorState == EDoorState::Closing) return;

	SetDoorState(EDoorState::Closing);
}

void ADRAutoSlidingDoor::SetDoorLocked(bool bLocked)
{
	if (!HasAuthority()) return;

	bIsLocked = bLocked;

	// 잠긴 상태에서 열려있으면 닫기
	if (bIsLocked && (CurrentDoorState == EDoorState::Open || CurrentDoorState == EDoorState::Opening))
	{
		CloseDoor();
	}
}

void ADRAutoSlidingDoor::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 플레이어인지 확인
	ADRCharacter* Character = Cast<ADRCharacter>(OtherActor);
	if (!Character) return;

	PlayersInTrigger++;

	if (bCloseOnPlayerOverlap)
	{
		CloseDoor();
	}
}

void ADRAutoSlidingDoor::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// 플레이어인지 확인
	ADRCharacter* Character = Cast<ADRCharacter>(OtherActor);
	if (!Character) return;

	PlayersInTrigger = FMath::Max(0, PlayersInTrigger - 1);

	if (bOpenOnPlayerLeave && PlayersInTrigger == 0)
	{
		OpenDoor();
	}
}

