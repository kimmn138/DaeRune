// Copyright DaeRune

#include "Actor/Stage2/DRS2CctvBoard.h"

#include "Engine/Texture.h"
#include "Game/DRStageGameState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"

ADRS2CctvBoard::ADRS2CctvBoard()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));

	// 장면 사진 6장이 머티리얼 슬롯으로 나뉜 단일 메시. BP 에서 메시 에셋을 지정한다.
	ScreenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenMesh"));
	ScreenMesh->SetupAttachment(GetRootComponent());
	ScreenMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 찾아야 할 캐릭터 그림을 보여주는 표시판. BP 에서 메시를 지정하고 위치를 잡는다.
	HintMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HintMesh"));
	HintMesh->SetupAttachment(GetRootComponent());
	HintMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRS2CctvBoard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2CctvBoard, Sequence);
	DOREPLIFETIME(ADRS2CctvBoard, StartServerTime);
	DOREPLIFETIME(ADRS2CctvBoard, ChosenTargetIndex);
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
			MID->SetTextureParameterValue(ScreenTextureParameterName, ErrorImage.Get());
		}
	}

	// 타깃이 정해지기 전에는 힌트판도 오류 화면으로 둔다.
	if (UMaterialInstanceDynamic* MID = EnsureHintMID())
	{
		MID->SetTextureParameterValue(
			HintTextureParameterName.IsNone() ? ScreenTextureParameterName : HintTextureParameterName,
			ErrorImage.Get());
	}

	// 타깃이 이미 복제되어 있으면(판 도중에 접속한 클라 등) 바로 반영한다.
	// BeginPlay 와 OnRep_ChosenTargetIndex 중 어느 쪽이 먼저 오든 나중 것이 채운다.
	ApplyHintTexture();
}

// ==================== 타깃 선정 ====================

void ADRS2CctvBoard::ChooseTarget()
{
	if (!HasAuthority()) return;

	if (TargetCandidates.Num() == 0)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2CCTV] TargetCandidates 가 비어 있어 타깃을 정할 수 없습니다. ")
			TEXT("BP 에 찾을 캐릭터 그림과 등장 수를 채우세요."));
		return;
	}

	ChosenTargetIndex = FMath::RandRange(0, TargetCandidates.Num() - 1);
	const FS2CctvTarget& Chosen = TargetCandidates[ChosenTargetIndex];

	if (!Chosen.TargetImage)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[S2CCTV] 후보 %d 에 TargetImage 가 없습니다. 힌트판이 비어 보입니다."), ChosenTargetIndex);
	}

	UE_LOG(LogDR, Log, TEXT("[S2CCTV] 후보 %d개 중 %d번을 선정했습니다. 금고 마지막 자리 = %d"),
		TargetCandidates.Num(), ChosenTargetIndex, Chosen.AppearCount);

	// 서버는 OnRep 이 불리지 않으므로 직접 반영한다
	OnRep_ChosenTargetIndex();

	BuildSequence();
}

int32 ADRS2CctvBoard::GetSecretDigit() const
{
	if (!TargetCandidates.IsValidIndex(ChosenTargetIndex))
	{
		return INDEX_NONE;
	}

	return TargetCandidates[ChosenTargetIndex].AppearCount;
}

// ==================== 시퀀스 ====================

void ADRS2CctvBoard::BuildSequence()
{
	if (!HasAuthority()) return;

	// 스텝마다 서로 다른 화면 2개가 켜져야 한다
	if (ScreenCount < 2)
	{
		UE_LOG(LogDR, Error, TEXT("[S2CCTV] 화면이 %d개뿐입니다. 최소 2개가 필요합니다."), ScreenCount);
		return;
	}

	const int32 SafeStepCount = FMath::Max(1, StepCount);
	const int32 SlotTotal = SafeStepCount * 2; // 스텝당 화면 2개

	// ★1주기 안에 모든 화면이 나와야 한다. 안 나오는 사진이 있으면 그 안의 캐릭터를 셀 수 없다.
	if (SlotTotal < ScreenCount)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2CCTV] 노출 슬롯이 %d개(스텝 %d)뿐이라 화면 %d개를 다 보여줄 수 없습니다. StepCount 를 늘리세요."),
			SlotTotal, SafeStepCount, ScreenCount);
	}

	// ① 화면을 골고루 담은 슬롯 배열을 만든다 (각 화면이 최소 SlotTotal/ScreenCount 번씩)
	TArray<int32> Slots;
	Slots.Reserve(SlotTotal);
	for (int32 i = 0; i < SlotTotal; ++i)
	{
		Slots.Add(i % ScreenCount);
	}

	// ② 셔플
	for (int32 i = Slots.Num() - 1; i > 0; --i)
	{
		Slots.Swap(i, FMath::RandRange(0, i));
	}

	// ③ 한 스텝에 같은 화면이 두 번 들어간 경우를 교환으로 푼다
	for (int32 Step = 0; Step < SafeStepCount; ++Step)
	{
		const int32 A = Step * 2;
		const int32 B = Step * 2 + 1;
		if (Slots[A] != Slots[B]) continue;

		bool bResolved = false;
		for (int32 Other = 0; Other < Slots.Num() && !bResolved; ++Other)
		{
			if (Other == A || Other == B) continue;

			// Other 의 짝 (같은 스텝의 반대쪽 슬롯)
			const int32 OtherPartner = (Other % 2 == 0) ? Other + 1 : Other - 1;

			// B 와 Other 를 바꿨을 때 양쪽 스텝 모두 서로 다른 화면이 되는가
			if (Slots[Other] != Slots[A] && Slots[B] != Slots[OtherPartner])
			{
				Slots.Swap(B, Other);
				bResolved = true;
			}
		}

		if (!bResolved)
		{
			UE_LOG(LogDR, Warning,
				TEXT("[S2CCTV] 스텝 %d 의 화면 중복을 풀지 못했습니다 (화면 %d개, 스텝 %d개)."),
				Step, ScreenCount, SafeStepCount);
		}
	}

	// ④ 스텝으로 묶는다
	Sequence.Reset();
	Sequence.Reserve(SafeStepCount);
	for (int32 Step = 0; Step < SafeStepCount; ++Step)
	{
		FS2CctvStep NewStep;
		NewStep.NormalScreenA = static_cast<uint8>(Slots[Step * 2]);
		NewStep.NormalScreenB = static_cast<uint8>(Slots[Step * 2 + 1]);
		Sequence.Add(NewStep);
	}

	// ⑤ 재생 기준 시각 (클라는 이 값과 서버시각으로 현재 스텝을 계산한다)
	if (const ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>())
	{
		StartServerTime = DRGameState->GetServerWorldTimeSeconds();
	}

	LastAppliedStep = INDEX_NONE;
	OnRep_Sequence();

	UE_LOG(LogDR, Log, TEXT("[S2CCTV] 시퀀스 생성: %d스텝 (%.0f초 주기), 화면 %d개"),
		Sequence.Num(), Sequence.Num() * StepDuration, ScreenCount);
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

		// 켜진 화면은 자기 장면 사진으로, 나머지는 오류 화면으로
		UTexture* Texture = ErrorImage.Get();
		if (i == Step.NormalScreenA || i == Step.NormalScreenB)
		{
			Texture = ScreenBaseTextures.IsValidIndex(i) ? ScreenBaseTextures[i].Get() : nullptr;
		}

		MID->SetTextureParameterValue(ScreenTextureParameterName, Texture);
	}
}

// ==================== 머티리얼 ====================

void ADRS2CctvBoard::EnsureScreenMIDs()
{
	ScreenMIDs.Reset();
	ScreenBaseTextures.Reset();

	if (!ScreenMesh || !ScreenMesh->GetStaticMesh())
	{
		UE_LOG(LogDR, Error, TEXT("[S2CCTV] ScreenMesh 에 메시가 지정되지 않았습니다."));
		return;
	}

	const int32 SlotTotal = ScreenMesh->GetNumMaterials();

	// 화면 인덱스마다 대응하는 머티리얼 슬롯의 MID 를 만들고, 그 슬롯의 원래 사진을 기억해 둔다.
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
			ScreenBaseTextures.Add(nullptr);
			continue;
		}

		UMaterialInstanceDynamic* MID = ScreenMesh->CreateAndSetMaterialInstanceDynamic(Slot);
		if (!MID)
		{
			UE_LOG(LogDR, Error,
				TEXT("[S2CCTV] 화면 %d (슬롯 %d): MID 생성 실패. 그 슬롯에 머티리얼이 있는지 확인하세요."),
				ScreenIndex, Slot);
			ScreenMIDs.Add(nullptr);
			ScreenBaseTextures.Add(nullptr);
			continue;
		}

		// ★이 화면이 켜졌을 때 되돌릴 원래 사진. 머티리얼에 이미 지정된 텍스처를 그대로 쓴다.
		UTexture* BaseTexture = MID->K2_GetTextureParameterValue(ScreenTextureParameterName);
		if (!BaseTexture)
		{
			UE_LOG(LogDR, Warning,
				TEXT("[S2CCTV] 화면 %d (슬롯 %d): 머티리얼에서 텍스처 파라미터 '%s' 를 찾지 못했습니다. ")
				TEXT("ScreenTextureParameterName 이 머티리얼의 파라미터 이름과 같은지 확인하세요 (glTF 임포트 머티리얼은 BaseColorTexture)."),
				ScreenIndex, Slot, *ScreenTextureParameterName.ToString());
		}

		ScreenMIDs.Add(MID);
		ScreenBaseTextures.Add(BaseTexture);
	}

	if (SlotTotal < ScreenCount)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2CCTV] 메시의 머티리얼 슬롯이 %d개인데 화면은 %d개입니다. 슬롯이 부족합니다."),
			SlotTotal, ScreenCount);
	}
}

UMaterialInstanceDynamic* ADRS2CctvBoard::EnsureHintMID()
{
	if (HintMID) return HintMID;

	// 힌트판은 선택 사항이다. 메시를 지정하지 않았으면 조용히 넘어간다.
	if (!HintMesh || !HintMesh->GetStaticMesh()) return nullptr;

	if (HintMaterialSlot < 0 || HintMaterialSlot >= HintMesh->GetNumMaterials())
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2CCTV] 힌트판: HintMaterialSlot %d 이 범위(0~%d) 밖입니다."),
			HintMaterialSlot, HintMesh->GetNumMaterials() - 1);
		return nullptr;
	}

	HintMID = HintMesh->CreateAndSetMaterialInstanceDynamic(HintMaterialSlot);
	if (!HintMID)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2CCTV] 힌트판: 슬롯 %d 의 MID 생성 실패. 그 슬롯에 머티리얼이 있는지 확인하세요."),
			HintMaterialSlot);
		return nullptr;
	}

	// 비워두면 화면과 같은 파라미터 이름을 쓴다
	const FName ParameterName = HintTextureParameterName.IsNone()
		? ScreenTextureParameterName
		: HintTextureParameterName;

	if (!HintMID->K2_GetTextureParameterValue(ParameterName))
	{
		UE_LOG(LogDR, Warning,
			TEXT("[S2CCTV] 힌트판: 머티리얼에서 텍스처 파라미터 '%s' 를 찾지 못했습니다. ")
			TEXT("HintTextureParameterName 을 확인하세요."),
			*ParameterName.ToString());
	}

	return HintMID;
}

void ADRS2CctvBoard::OnRep_ChosenTargetIndex()
{
	ApplyHintTexture();
}

void ADRS2CctvBoard::ApplyHintTexture()
{
	// 아직 타깃이 안 정해졌으면(페이즈 주입 전) 조용히 넘어간다.
	// BeginPlay 와 OnRep_ChosenTargetIndex 중 나중에 오는 쪽이 채운다.
	if (!TargetCandidates.IsValidIndex(ChosenTargetIndex)) return;

	UTexture2D* TargetImage = TargetCandidates[ChosenTargetIndex].TargetImage;
	if (!TargetImage) return;   // ChooseTarget() 에서 이미 경고했다

	UMaterialInstanceDynamic* MID = EnsureHintMID();
	if (!MID) return;

	const FName ParameterName = HintTextureParameterName.IsNone()
		? ScreenTextureParameterName
		: HintTextureParameterName;

	MID->SetTextureParameterValue(ParameterName, TargetImage);

	UE_LOG(LogDR, Log, TEXT("[S2CCTV] 힌트판: 후보 %d번의 캐릭터 그림을 표시합니다."), ChosenTargetIndex);
}
