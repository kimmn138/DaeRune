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
	// BP에서 태그를 단 화면 메시들을 수집한다 (이름 순 정렬 = 화면 인덱스 순서)
	Screens.Reset();

	TArray<UStaticMeshComponent*> FoundComponents;
	GetComponents<UStaticMeshComponent>(FoundComponents);

	for (UStaticMeshComponent* Component : FoundComponents)
	{
		if (Component && Component->ComponentHasTag(ScreenComponentTag))
		{
			Screens.Add(Component);
		}
	}

	Screens.Sort([](const TObjectPtr<UStaticMeshComponent>& A, const TObjectPtr<UStaticMeshComponent>& B)
	{
		return A->GetName() < B->GetName();
	});

	if (Screens.Num() == 0)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2CCTV] 화면 메시를 찾지 못했습니다. BP 컴포넌트에 '%s' 태그를 다세요."),
			*ScreenComponentTag.ToString());
	}

	ScreenMIDs.Reset();
	for (UStaticMeshComponent* Screen : Screens)
	{
		ScreenMIDs.Add(Screen ? Screen->CreateAndSetMaterialInstanceDynamic(0) : nullptr);
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

	const int32 ScreenCount = Screens.Num();
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
