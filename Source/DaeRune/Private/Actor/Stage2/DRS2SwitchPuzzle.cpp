// Copyright DaeRune

#include "Actor/Stage2/DRS2SwitchPuzzle.h"

#include "Actor/Stage2/DRS2CodeScreen.h"
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

	if (HasAuthority())
	{
		GenerateRound();
	}
}

void ADRS2SwitchPuzzle::SetRevealDigit(int32 InDigit)
{
	if (!HasAuthority()) return;

	SecretDigit = InDigit;
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

bool ADRS2SwitchPuzzle::HasSolution() const
{
	const int32 AllOn = GetAllBulbsOnMask();
	const int32 ComboCount = 1 << LeverMasks.Num();

	// 공집합(아무 레버도 안 켠 상태)은 제외하고 완전탐색
	for (int32 Combo = 1; Combo < ComboCount; ++Combo)
	{
		int32 Bits = 0;
		for (int32 i = 0; i < LeverMasks.Num(); ++i)
		{
			if (Combo & (1 << i))
			{
				Bits ^= LeverMasks[i];
			}
		}

		if (Bits == AllOn) return true;
	}

	return false;
}

void ADRS2SwitchPuzzle::GenerateRound()
{
	if (!HasAuthority()) return;

	const int32 AllOn = GetAllBulbsOnMask();
	const int32 SafeLeverCount = FMath::Max(1, LeverCount);

	bool bGenerated = false;

	// 정답 부분집합에서 역산 -> 완전탐색 검증. 실패하면 재시도한다.
	for (int32 Attempt = 0; Attempt < 32 && !bGenerated; ++Attempt)
	{
		LeverMasks.Reset();
		LeverMasks.SetNum(SafeLeverCount);

		// ① 정답이 될 레버 부분집합 S 선택 (공집합이 아니어야 한다)
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

		// ② 일단 전 레버에 자유 랜덤 마스크를 채운다
		for (int32 i = 0; i < SafeLeverCount; ++i)
		{
			LeverMasks[i] = MakeRandomMask();
		}

		// ③ S의 마지막 원소를 역산해 S 전체의 XOR 이 "전구 전부 켜짐"이 되게 만든다
		int32 Accumulated = 0;
		for (int32 k = 0; k < SolutionSet.Num() - 1; ++k)
		{
			Accumulated ^= LeverMasks[SolutionSet[k]];
		}
		LeverMasks[SolutionSet.Last()] = AllOn ^ Accumulated;

		// ④ 퇴화 마스크(아무 전구도 담당하지 않음) 검사
		bool bDegenerate = false;
		for (int32 Mask : LeverMasks)
		{
			if (Mask == 0)
			{
				bDegenerate = true;
				break;
			}
		}

		// ⑤ 완전탐색 검증 (레버 5개면 32조합이라 비용 무시 가능)
		bGenerated = !bDegenerate && HasSolution();
	}

	if (!bGenerated)
	{
		// 여기까지 오면 설정값이 비정상이다. 최소한 풀 수 있는 상태로 강제한다.
		UE_LOG(LogDR, Error,
			TEXT("[S2Switch] 해가 있는 마스크 생성에 실패했습니다. 레버 %d / 전구 %d 설정을 확인하세요."),
			SafeLeverCount, BulbCount);

		LeverMasks.Reset();
		LeverMasks.SetNum(SafeLeverCount);
		LeverMasks[0] = AllOn;
		for (int32 i = 1; i < SafeLeverCount; ++i)
		{
			LeverMasks[i] = 1 << (i % BulbCount);
		}
	}

	// 레버를 모두 내린 상태로 라운드 시작
	LeverBits = 0;
	BulbBits = 0;
	OnRep_LeverBits();
	OnRep_BulbBits();

	UE_LOG(LogDR, Verbose, TEXT("[S2Switch] 라운드 생성 (성공 %d/%d)"), RoundsCleared, RequiredRounds);
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

void ADRS2SwitchPuzzle::OnRep_LeverBits()
{
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
