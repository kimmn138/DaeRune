// Copyright DaeRune

#include "Actor/Stage2/DRS2CctvBoard.h"

#include "Game/DRStageGameState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"

ADRS2CctvBoard::ADRS2CctvBoard()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));

	// 화면 6개가 머티리얼 슬롯으로 나뉜 단일 메시. BP 에서 메시 에셋을 지정한다.
	ScreenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenMesh"));
	ScreenMesh->SetupAttachment(GetRootComponent());
	ScreenMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRS2CctvBoard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2CctvBoard, Sequence);
	DOREPLIFETIME(ADRS2CctvBoard, StartServerTime);
}

void ADRS2CctvBoard::BeginPlay()
{
	Super::BeginPlay();

	EnsureScreenMIDs();

	// 시퀀스가 시작되기 전에는 전 화면을 오류 화면으로 둔다
	for (UMaterialInstanceDynamic* MID : ScreenMIDs)
	{
		if (MID)
		{
			MID->SetTextureParameterValue(ScreenTextureParameterName, ErrorImage);
		}
	}
}

void ADRS2CctvBoard::EnsureScreenMIDs()
{
	ScreenMIDs.Reset();

	if (!ScreenMesh || !ScreenMesh->GetStaticMesh())
	{
		UE_LOG(LogDR, Error, TEXT("[S2CCTV] ScreenMesh 에 메시가 지정되지 않았습니다."));
		return;
	}

	const int32 SlotTotal = ScreenMesh->GetNumMaterials();

	// 화면 인덱스마다 대응하는 머티리얼 슬롯의 MID 를 만든다.
	for (int32 ScreenIndex = 0; ScreenIndex < ScreenCount; ++ScreenIndex)
	{
		// 매핑을 비워두면 슬롯 번호 = 화면 번호
		const int32 Slot = ScreenMaterialSlots.IsValidIndex(ScreenIndex)
			? ScreenMaterialSlots[ScreenIndex]
			: ScreenIndex;

		if (Slot < 0 || Slot >= SlotTotal)
		{
			UE_LOG(LogDR, Error,
				TEXT("[S2CCTV] 화면 %d 의 머티리얼 슬롯 %d 이 범위(0~%d) 밖입니다."),
				ScreenIndex, Slot, SlotTotal - 1);
			ScreenMIDs.Add(nullptr);
			continue;
		}

		UMaterialInstanceDynamic* MID = ScreenMesh->CreateAndSetMaterialInstanceDynamic(Slot);
		if (!MID)
		{
			UE_LOG(LogDR, Error,
				TEXT("[S2CCTV] 화면 %d (슬롯 %d): MID 생성 실패. 그 슬롯에 머티리얼이 있는지 확인하세요."),
				ScreenIndex, Slot);
		}

		ScreenMIDs.Add(MID);
	}

	if (SlotTotal < ScreenCount)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2CCTV] 메시의 머티리얼 슬롯이 %d개인데 화면은 %d개입니다. 슬롯이 부족합니다."),
			SlotTotal, ScreenCount);
	}
}

void ADRS2CctvBoard::SetTargetImageCount(int32 InCount)
{
	if (!HasAuthority()) return;

	TargetImageCount = FMath::Max(0, InCount);
	BuildSequence();
}

void ADRS2CctvBoard::BuildSequence()
{
	if (!HasAuthority()) return;

	if (ScreenCount < 2)
	{
		UE_LOG(LogDR, Error, TEXT("[S2CCTV] 화면이 %d개뿐입니다. 최소 2개가 필요합니다."), ScreenCount);
		return;
	}

	const int32 SafeStepCount = FMath::Max(1, StepCount);
	const int32 SlotTotal = SafeStepCount * 2; // 스텝당 정상 화면 2개

	if (TargetImageCount > SlotTotal)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[S2CCTV] 타깃 %d개 > 노출 슬롯 %d개. StepCount 를 늘리세요."), TargetImageCount, SlotTotal);
	}

	// ① 슬롯 배열 구성: 타깃 N개 + 나머지는 더미
	TArray<uint8> Slots;
	Slots.Reserve(SlotTotal);

	const int32 ClampedTargetCount = FMath::Clamp(TargetImageCount, 0, SlotTotal);
	for (int32 i = 0; i < SlotTotal; ++i)
	{
		if (i < ClampedTargetCount)
		{
			Slots.Add(0); // 0 = 타깃 이미지
		}
		else
		{
			const int32 DummyIndex = DummyImages.Num() > 0 ? FMath::RandRange(0, DummyImages.Num() - 1) : 0;
			Slots.Add(static_cast<uint8>(1 + DummyIndex));
		}
	}

	// ② 셔플 (타깃이 앞쪽에 몰리지 않게)
	for (int32 i = Slots.Num() - 1; i > 0; --i)
	{
		Slots.Swap(i, FMath::RandRange(0, i));
	}

	// ③ 스텝별로 정상 화면 2개 선택 + 이미지 할당
	Sequence.Reset();
	Sequence.Reserve(SafeStepCount);

	for (int32 Step = 0; Step < SafeStepCount; ++Step)
	{
		FS2CctvStep NewStep;
		NewStep.NormalScreenA = static_cast<uint8>(FMath::RandRange(0, ScreenCount - 1));

		int32 ScreenB = FMath::RandRange(0, ScreenCount - 1);
		int32 Guard = 0;
		while (ScreenB == NewStep.NormalScreenA && Guard++ < 16)
		{
			ScreenB = FMath::RandRange(0, ScreenCount - 1);
		}
		NewStep.NormalScreenB = static_cast<uint8>(ScreenB);

		NewStep.ImageA = Slots[Step * 2];
		NewStep.ImageB = Slots[Step * 2 + 1];

		Sequence.Add(NewStep);
	}

	// ④ 재생 기준 시각 (클라는 이 값과 서버시각으로 현재 스텝을 계산한다)
	if (const ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>())
	{
		StartServerTime = DRGameState->GetServerWorldTimeSeconds();
	}

	LastAppliedStep = INDEX_NONE;
	OnRep_Sequence();

	UE_LOG(LogDR, Log, TEXT("[S2CCTV] 시퀀스 생성: %d스텝 (%d슬롯), 타깃 %d개"),
		Sequence.Num(), SlotTotal, ClampedTargetCount);
}

void ADRS2CctvBoard::OnRep_Sequence()
{
	// 시퀀스가 도착하면 재생을 시작한다 (서버/클라 공통)
	LastAppliedStep = INDEX_NONE;
}

void ADRS2CctvBoard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Sequence.Num() == 0) return;

	const ADRStageGameState* DRGameState = GetWorld() ? GetWorld()->GetGameState<ADRStageGameState>() : nullptr;
	if (!DRGameState) return;

	const float Now = DRGameState->GetServerWorldTimeSeconds();
	const float Elapsed = Now - StartServerTime;
	if (Elapsed < 0.f) return;

	// 시퀀스가 끝나면 처음부터 반복한다 (놓쳐도 다시 셀 수 있어야 하므로)
	const int32 StepIndex = FMath::FloorToInt(Elapsed / FMath::Max(StepDuration, KINDA_SMALL_NUMBER)) % Sequence.Num();

	if (StepIndex != LastAppliedStep)
	{
		LastAppliedStep = StepIndex;
		ApplyStep(StepIndex);
	}
}

void ADRS2CctvBoard::ApplyStep(int32 StepIndex)
{
	if (!Sequence.IsValidIndex(StepIndex)) return;

	const FS2CctvStep& Step = Sequence[StepIndex];

	for (int32 i = 0; i < ScreenMIDs.Num(); ++i)
	{
		UMaterialInstanceDynamic* MID = ScreenMIDs[i];
		if (!MID) continue;

		UTexture2D* Texture = ErrorImage;
		if (i == Step.NormalScreenA)
		{
			Texture = IndexToTexture(Step.ImageA);
		}
		else if (i == Step.NormalScreenB)
		{
			Texture = IndexToTexture(Step.ImageB);
		}

		MID->SetTextureParameterValue(ScreenTextureParameterName, Texture);
	}
}

UTexture2D* ADRS2CctvBoard::IndexToTexture(uint8 ImageIndex) const
{
	if (ImageIndex == 0)
	{
		return TargetImage;
	}

	const int32 DummyIndex = static_cast<int32>(ImageIndex) - 1;
	return DummyImages.IsValidIndex(DummyIndex) ? DummyImages[DummyIndex] : ErrorImage;
}
