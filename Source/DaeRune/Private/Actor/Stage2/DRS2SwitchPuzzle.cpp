// Copyright DaeRune

#include "Actor/Stage2/DRS2SwitchPuzzle.h"

#include "Actor/Stage2/DRS2CodeScreen.h"
#include "Actor/Stage2/DRS2InteractProp.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"

ADRS2SwitchPuzzle::ADRS2SwitchPuzzle()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ADRS2SwitchPuzzle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2SwitchPuzzle, BulbBits);
	DOREPLIFETIME(ADRS2SwitchPuzzle, LeverBits);
	DOREPLIFETIME(ADRS2SwitchPuzzle, RoundsCleared);
	DOREPLIFETIME(ADRS2SwitchPuzzle, RevealedDigit);
	DOREPLIFETIME(ADRS2SwitchPuzzle, bSolved);
}

void ADRS2SwitchPuzzle::BeginPlay()
{
	Super::BeginPlay();

	if (CodeScreen)
	{
		CodeScreen->SetDigits({ -1 });
	}

	// ★클라에서는 초기 복제가 BeginPlay 보다 먼저 도착할 수 있다.
	//   그때 발화한 OnRep 은 BP 가 아직 준비되지 않은(MID·배열이 빈) 상태라 그려지지 않는다.
	//   Super::BeginPlay() 로 BP 의 Event BeginPlay 가 끝난 지금 한 번 더 밀어준다.
	OnBulbsChanged(BulbBits);
	OnLeversChanged(LeverBits);
	OnRoundsClearedChanged(RoundsCleared);

	if (HasAuthority())
	{
		// 프리셋 검증을 먼저 끝내야 첫 라운드에서 쓸 수 있다.
		BuildValidPresets();
		GenerateRound();
	}
}

void ADRS2SwitchPuzzle::SetRevealDigit(int32 InDigit)
{
	if (!HasAuthority()) return;

	SecretDigit = InDigit;
}

bool ADRS2SwitchPuzzle::IsBitSet(int32 Bits, int32 Index)
{
	if (Index < 0 || Index >= 31) return false;

	return (Bits & (1 << Index)) != 0;
}

int32 ADRS2SwitchPuzzle::MakeRandomMask() const
{
	const int32 AllOn = GetAllBulbsOnMask();
	const int32 MinBits = FMath::Clamp(MinBitsPerLever, 1, BulbCount);
	const int32 MaxBits = FMath::Clamp(MaxBitsPerLever, MinBits, BulbCount);
	const int32 TargetBits = FMath::RandRange(MinBits, MaxBits);

	int32 Mask = 0;
	int32 BitsSet = 0;
	int32 Guard = 0;

	while (BitsSet < TargetBits && Guard++ < 256)
	{
		const int32 Bit = 1 << FMath::RandRange(0, BulbCount - 1);
		if ((Mask & Bit) == 0)
		{
			Mask |= Bit;
			++BitsSet;
		}
	}

	// 전부 켜진 마스크는 단독 정답이 되어 퍼즐이 무의미해지므로 회피
	return (Mask == AllOn) ? (Mask & ~1) : Mask;
}

bool ADRS2SwitchPuzzle::HasSolution(const TArray<int32>& Masks) const
{
	const int32 AllOn = GetAllBulbsOnMask();
	const int32 ComboCount = 1 << Masks.Num();

	// 공집합(아무 레버도 안 켠 상태)은 제외하고 완전탐색
	for (int32 Combo = 1; Combo < ComboCount; ++Combo)
	{
		int32 Bits = 0;
		for (int32 i = 0; i < Masks.Num(); ++i)
		{
			if (Combo & (1 << i))
			{
				Bits ^= Masks[i];
			}
		}

		if (Bits == AllOn) return true;
	}

	return false;
}

// ================= 프리셋 =================

int32 ADRS2SwitchPuzzle::MaskFromBulbIndices(const TArray<int32>& BulbIndices, int32 PresetIndex, int32 LeverIndex) const
{
	int32 Mask = 0;

	for (int32 BulbIndex : BulbIndices)
	{
		if (BulbIndex < 0 || BulbIndex >= BulbCount)
		{
			UE_LOG(LogDR, Warning,
				TEXT("[S2Switch] 프리셋 %d / 레버 %d: 전구 번호 %d 는 범위(0~%d) 밖이라 무시합니다."),
				PresetIndex, LeverIndex, BulbIndex, BulbCount - 1);
			continue;
		}

		Mask |= (1 << BulbIndex);
	}

	return Mask;
}

void ADRS2SwitchPuzzle::BuildValidPresets()
{
	ValidPresetMasks.Reset();
	RemainingPresetIndices.Reset();

	for (int32 PresetIndex = 0; PresetIndex < Presets.Num(); ++PresetIndex)
	{
		const FS2SwitchPreset& Preset = Presets[PresetIndex];

		if (Preset.Levers.Num() != LeverCount)
		{
			UE_LOG(LogDR, Error,
				TEXT("[S2Switch] 프리셋 %d: 레버 항목이 %d개인데 LeverCount 는 %d 입니다. 이 프리셋을 건너뜁니다."),
				PresetIndex, Preset.Levers.Num(), LeverCount);
			continue;
		}

		TArray<int32> Masks;
		Masks.Reserve(LeverCount);

		bool bDegenerate = false;
		for (int32 LeverIndex = 0; LeverIndex < Preset.Levers.Num(); ++LeverIndex)
		{
			const int32 Mask = MaskFromBulbIndices(Preset.Levers[LeverIndex].BulbIndices, PresetIndex, LeverIndex);

			// 담당 전구가 하나도 없는 레버는 눌러도 아무 일이 없어 플레이어를 혼란시킨다.
			if (Mask == 0)
			{
				UE_LOG(LogDR, Error,
					TEXT("[S2Switch] 프리셋 %d / 레버 %d: 담당 전구가 없습니다. 이 프리셋을 건너뜁니다."),
					PresetIndex, LeverIndex);
				bDegenerate = true;
				break;
			}

			Masks.Add(Mask);
		}

		if (bDegenerate) continue;

		// ★가장 중요한 검증: 전구를 전부 켜는 조합이 실제로 존재하는가.
		//   없으면 그 라운드는 영원히 못 풀어 소프트락이 된다.
		if (!HasSolution(Masks))
		{
			UE_LOG(LogDR, Error,
				TEXT("[S2Switch] 프리셋 %d: 전구 %d개를 모두 켜는 레버 조합이 존재하지 않습니다. 이 프리셋을 건너뜁니다."),
				PresetIndex, BulbCount);
			continue;
		}

		ValidPresetMasks.Add(MoveTemp(Masks));
	}

	if (ValidPresetMasks.Num() == 0)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[S2Switch] 사용 가능한 프리셋이 없습니다 (등록 %d개). %s"),
			Presets.Num(),
			bAllowProceduralFallback ? TEXT("랜덤 생성으로 대체합니다.") : TEXT("bAllowProceduralFallback 이 꺼져 있어 퍼즐이 동작하지 않습니다."));
	}
	else
	{
		UE_LOG(LogDR, Log, TEXT("[S2Switch] 프리셋 %d/%d개 사용 가능"), ValidPresetMasks.Num(), Presets.Num());

		if (ValidPresetMasks.Num() < RequiredRounds)
		{
			UE_LOG(LogDR, Warning,
				TEXT("[S2Switch] 프리셋이 %d개뿐이라 %d라운드 중 같은 조합이 반복됩니다. %d개 이상 권장."),
				ValidPresetMasks.Num(), RequiredRounds, RequiredRounds);
		}
	}
}

bool ADRS2SwitchPuzzle::PickPresetMasks(TArray<int32>& OutMasks)
{
	if (ValidPresetMasks.Num() == 0) return false;

	// 풀이 비면 전체 인덱스로 다시 채운다 (모두 한 번씩 쓴 뒤 반복).
	if (RemainingPresetIndices.Num() == 0)
	{
		for (int32 Index = 0; Index < ValidPresetMasks.Num(); ++Index)
		{
			RemainingPresetIndices.Add(Index);
		}
	}

	const int32 PickedSlot = FMath::RandRange(0, RemainingPresetIndices.Num() - 1);
	const int32 PresetIndex = RemainingPresetIndices[PickedSlot];
	RemainingPresetIndices.RemoveAt(PickedSlot);

	OutMasks = ValidPresetMasks[PresetIndex];

	UE_LOG(LogDR, Verbose, TEXT("[S2Switch] 프리셋 %d 사용"), PresetIndex);
	return true;
}

// ================= 라운드 =================

void ADRS2SwitchPuzzle::GenerateRound()
{
	if (!HasAuthority()) return;

	// 프리셋 우선. 없으면(그리고 허용되면) 절차적 생성으로 대체한다.
	if (!PickPresetMasks(LeverMasks))
	{
		if (bAllowProceduralFallback)
		{
			GenerateProceduralMasks(LeverMasks);
		}
		else
		{
			LeverMasks.Reset();
			UE_LOG(LogDR, Error, TEXT("[S2Switch] 사용할 조합이 없어 라운드를 시작할 수 없습니다."));
		}
	}

	// 레버를 모두 내린 상태로 라운드 시작
	LeverBits = 0;
	BulbBits = 0;
	OnRep_LeverBits();
	OnRep_BulbBits();

	UE_LOG(LogDR, Verbose, TEXT("[S2Switch] 라운드 생성 (성공 %d/%d)"), RoundsCleared, RequiredRounds);
}

void ADRS2SwitchPuzzle::GenerateProceduralMasks(TArray<int32>& OutMasks) const
{
	const int32 AllOn = GetAllBulbsOnMask();
	const int32 SafeLeverCount = FMath::Max(1, LeverCount);

	bool bGenerated = false;

	// 정답 부분집합에서 역산 -> 완전탐색 검증. 실패하면 재시도한다.
	for (int32 Attempt = 0; Attempt < 32 && !bGenerated; ++Attempt)
	{
		OutMasks.Reset();
		OutMasks.SetNum(SafeLeverCount);

		// (1) 정답이 될 레버 부분집합 S 선택 (공집합이 아니어야 한다)
		TArray<int32> SolutionSet;
		int32 Guard = 0;
		while (SolutionSet.Num() == 0 && Guard++ < 16)
		{
			for (int32 i = 0; i < SafeLeverCount; ++i)
			{
				if (FMath::RandBool())
				{
					SolutionSet.Add(i);
				}
			}
		}
		if (SolutionSet.Num() == 0)
		{
			SolutionSet.Add(FMath::RandRange(0, SafeLeverCount - 1));
		}

		// (2) 일단 전 레버에 자유 랜덤 마스크를 채운다
		for (int32 i = 0; i < SafeLeverCount; ++i)
		{
			OutMasks[i] = MakeRandomMask();
		}

		// (3) S의 마지막 원소를 역산해 S 전체의 XOR 이 "전구 전부 켜짐"이 되게 만든다
		int32 Accumulated = 0;
		for (int32 k = 0; k < SolutionSet.Num() - 1; ++k)
		{
			Accumulated ^= OutMasks[SolutionSet[k]];
		}
		OutMasks[SolutionSet.Last()] = AllOn ^ Accumulated;

		// (4) 퇴화 마스크(아무 전구도 담당하지 않음) 검사
		bool bDegenerate = false;
		for (int32 Mask : OutMasks)
		{
			if (Mask == 0)
			{
				bDegenerate = true;
				break;
			}
		}

		// (5) 완전탐색 검증 (레버 5개면 32조합이라 비용 무시 가능)
		bGenerated = !bDegenerate && HasSolution(OutMasks);
	}

	if (!bGenerated)
	{
		// 여기까지 오면 설정값이 비정상이다. 최소한 풀 수 있는 상태로 강제한다.
		UE_LOG(LogDR, Error,
			TEXT("[S2Switch] 해가 있는 마스크 생성에 실패했습니다. 레버 %d / 전구 %d 설정을 확인하세요."),
			SafeLeverCount, BulbCount);

		OutMasks.Reset();
		OutMasks.SetNum(SafeLeverCount);
		OutMasks[0] = AllOn;
		for (int32 i = 1; i < SafeLeverCount; ++i)
		{
			OutMasks[i] = 1 << (i % BulbCount);
		}
	}
}

int32 ADRS2SwitchPuzzle::ComputeBulbBits() const
{
	int32 Bits = 0;
	for (int32 i = 0; i < LeverMasks.Num(); ++i)
	{
		if (LeverBits & (1 << i))
		{
			// XOR: 두 레버가 공유하는 전구는 서로 상쇄된다 (라이트아웃 방식)
			Bits ^= LeverMasks[i];
		}
	}
	return Bits;
}

void ADRS2SwitchPuzzle::ToggleLever(int32 LeverIndex)
{
	if (!HasAuthority() || bSolved) return;
	if (!LeverMasks.IsValidIndex(LeverIndex)) return;

	LeverBits ^= (1 << LeverIndex);
	BulbBits = ComputeBulbBits();

	OnRep_LeverBits();
	OnRep_BulbBits();

	CheckRound();
}

void ADRS2SwitchPuzzle::CheckRound()
{
	if (BulbBits != GetAllBulbsOnMask()) return;

	++RoundsCleared;
	OnRep_RoundsCleared();

	UE_LOG(LogDR, Log, TEXT("[S2Switch] 라운드 성공 %d/%d"), RoundsCleared, RequiredRounds);

	if (RoundsCleared >= RequiredRounds)
	{
		bSolved = true;
		RevealedDigit = SecretDigit;
		OnRep_RevealedDigit();

		OnPuzzleSolved.Broadcast(this);
	}
	else
	{
		// 다음 라운드: 레버-전구 조합을 새로 만들고 레버를 전부 내린다
		GenerateRound();
	}
}

void ADRS2SwitchPuzzle::OnRep_BulbBits()
{
	OnBulbsChanged(BulbBits);
}

void ADRS2SwitchPuzzle::RegisterLever(ADRS2Lever* Lever)
{
	if (!IsValid(Lever)) return;

	RegisteredLevers.AddUnique(Lever);

	// ★등록 즉시 현재 상태를 밀어준다.
	//   레버와 퍼즐의 BeginPlay 순서, 클라의 LeverBits 복제 도착 순서에 의존하지 않게 된다.
	Lever->SetLeverOn(IsBitSet(LeverBits, Lever->GetPropIndex()));
}

void ADRS2SwitchPuzzle::RefreshLeverPoses()
{
	for (const TObjectPtr<ADRS2Lever>& Lever : RegisteredLevers)
	{
		if (IsValid(Lever))
		{
			Lever->SetLeverOn(IsBitSet(LeverBits, Lever->GetPropIndex()));
		}
	}
}

void ADRS2SwitchPuzzle::OnRep_LeverBits()
{
	// 라운드 성공 시 GenerateRound 가 LeverBits 를 0 으로 되돌리는데,
	// 이 경로 덕분에 레버들이 자동으로 전부 내려간다.
	RefreshLeverPoses();

	OnLeversChanged(LeverBits);
}

void ADRS2SwitchPuzzle::OnRep_RoundsCleared()
{
	OnRoundsClearedChanged(RoundsCleared);
}

void ADRS2SwitchPuzzle::OnRep_RevealedDigit()
{
	if (CodeScreen)
	{
		CodeScreen->SetDigits({ RevealedDigit });
	}

	if (RevealedDigit >= 0)
	{
		OnPuzzleSolvedVisual(RevealedDigit);
	}
}
