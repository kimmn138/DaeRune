// Copyright DaeRune

#include "Actor/Stage2/DRS2Safe.h"

#include "Actor/DRCleanserPart.h"
#include "Actor/Stage2/DRS2CodeScreen.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"

ADRS2Safe::ADRS2Safe()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SafeBodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SafeBodyMesh"));
	SafeBodyMesh->SetupAttachment(SceneRoot);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(SafeBodyMesh);

	PartSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("PartSpawnPoint"));
	PartSpawnPoint->SetupAttachment(SceneRoot);
}

void ADRS2Safe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2Safe, InputDigits);
	DOREPLIFETIME(ADRS2Safe, bOpened);
}

void ADRS2Safe::BeginPlay()
{
	Super::BeginPlay();

	RefreshDisplay();
}

void ADRS2Safe::SetSecretCode(const TArray<uint8>& InCode)
{
	if (!HasAuthority()) return;

	SecretCode = InCode;
	CodeLength = SecretCode.Num();

	InputDigits.Reset();
	OnRep_InputDigits();
}

void ADRS2Safe::PushDigit(uint8 Digit)
{
	if (!HasAuthority() || bOpened) return;

	InputDigits.Add(Digit);
	OnRep_InputDigits();

	if (InputDigits.Num() >= CodeLength)
	{
		ValidateInput();
	}
}

void ADRS2Safe::ClearInput()
{
	if (!HasAuthority() || bOpened) return;

	InputDigits.Reset();
	OnRep_InputDigits();
}

void ADRS2Safe::ValidateInput()
{
	if (SecretCode.Num() == 0)
	{
		UE_LOG(LogDR, Error, TEXT("[S2Safe] 정답 코드가 주입되지 않았습니다. 페이즈 배선을 확인하세요."));
		InputDigits.Reset();
		OnRep_InputDigits();
		return;
	}

	const bool bMatched = (InputDigits == SecretCode);

	if (bMatched)
	{
		OpenSafe();
	}
	else
	{
		// 오답은 입력만 초기화한다. 페널티나 잠금 시간은 두지 않는다.
		InputDigits.Reset();
		OnRep_InputDigits();
		Multicast_PlayWrongCodeFX();

		UE_LOG(LogDR, Verbose, TEXT("[S2Safe] 오답 - 입력 초기화"));
	}
}

void ADRS2Safe::OpenSafe()
{
	if (bOpened) return;

	bOpened = true;
	OnRep_bOpened();

	// 내부 부품 스폰 -> 이후는 기존 픽업 파이프라인을 그대로 탄다
	AActor* SpawnedPart = nullptr;
	if (PartClass && PartSpawnPoint)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		SpawnedPart = GetWorld()->SpawnActor<ADRCleanserPart>(
			PartClass, PartSpawnPoint->GetComponentTransform(), SpawnParams);
	}
	else
	{
		UE_LOG(LogDR, Error, TEXT("[S2Safe] PartClass 가 지정되지 않아 부품을 스폰하지 못했습니다."));
	}

	UE_LOG(LogDR, Log, TEXT("[S2Safe] 금고 개방"));

	OnSafeOpened.Broadcast(SpawnedPart);
}

void ADRS2Safe::OnRep_InputDigits()
{
	RefreshDisplay();
}

void ADRS2Safe::OnRep_bOpened()
{
	if (bOpened)
	{
		OnSafeOpenedVisual();
	}
}

void ADRS2Safe::RefreshDisplay()
{
	if (!InputDisplay) return;

	// 입력된 자리는 숫자로, 남은 자리는 -1(미입력)로 표시한다
	TArray<int32> DisplayDigits;
	DisplayDigits.Reserve(CodeLength);

	for (int32 i = 0; i < CodeLength; ++i)
	{
		DisplayDigits.Add(InputDigits.IsValidIndex(i) ? static_cast<int32>(InputDigits[i]) : -1);
	}

	InputDisplay->SetDigits(DisplayDigits);
}

void ADRS2Safe::Multicast_PlayWrongCodeFX_Implementation()
{
	OnWrongCodeVisual();
}
