// Copyright DaeRune

#include "Actor/Stage2/DRS2SlidePuzzle.h"

#include "Actor/Stage2/DRS2SlidePuzzleTypes.h"
#include "Character/DRCharacter.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Player/DRPlayerController.h"
#include "DaeRune/DRLogChannels.h"

ADRS2SlidePuzzle::ADRS2SlidePuzzle()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ADRS2SlidePuzzle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2SlidePuzzle, Board);
	DOREPLIFETIME(ADRS2SlidePuzzle, MoveCount);
	DOREPLIFETIME(ADRS2SlidePuzzle, Occupant);
	DOREPLIFETIME(ADRS2SlidePuzzle, RevealedDigit);
	DOREPLIFETIME(ADRS2SlidePuzzle, bSolved);
}

void ADRS2SlidePuzzle::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	// ★판은 여기서 딱 한 번 만든다★
	// 창을 열 때마다 섞으면 사람이 바뀔 때마다 새 판이 나와 "진행도 공유"가 깨진다.
	Board      = DRS2Puzzle::MakeShuffledBoard(ShuffleSteps);
	StartBoard = Board;
	History.Empty();
	MoveCount = 0;

	// 서버에서는 RepNotify 가 저절로 불리지 않는다 (리슨 서버 호스트 화면도 이 값을 쓴다).
	OnRep_Board();
}

void ADRS2SlidePuzzle::SetRevealDigit(int32 InDigit)
{
	if (!HasAuthority()) return;

	SecretDigit = InDigit;
}

// ================= 점유권 =================

void ADRS2SlidePuzzle::RequestOpenUI(ADRCharacter* Character)
{
	if (!HasAuthority() || !IsValid(Character)) return;

	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(Character->GetController());
	if (!PlayerController)
	{
		UE_LOG(LogDR, Warning, TEXT("[S2Puzzle] %s 의 컨트롤러가 ADRPlayerController 가 아니다 - 창을 열 수 없다"),
			*GetNameSafe(Character));
		return;
	}

	// 1) 이미 풀린 판은 '번호 확인용'이다. 점유하지 않고 누구나, 몇 명이든 연다.
	//    (풀지 않은 사람이 금고 번호를 볼 방법이 이것뿐이다)
	if (bSolved)
	{
		PlayerController->Client_OpenSlidePuzzleUI(this);
		return;
	}

	// 2) 나간 사람이 붙들고 있던 점유는 흘려보낸다.
	//    PlayerState 가 파괴돼도 GC 전까지는 포인터가 남아 있으므로 IsValid 로 본다.
	if (Occupant && !IsValid(Occupant))
	{
		SetOccupant(nullptr);
	}

	// 3) 남이 조작 중이면 거절
	if (Occupant && Occupant != PlayerController->PlayerState)
	{
		UE_LOG(LogDR, Log, TEXT("[S2Puzzle] 8퍼즐 조작 거절 - %s 가 사용 중"), *GetNameSafe(Occupant));

		PlayerController->Client_SlidePuzzleBusy(this, Occupant);
		return;
	}

	SetOccupant(PlayerController->PlayerState);

	UE_LOG(LogDR, Log, TEXT("[S2Puzzle] %s 에게 8퍼즐 창 열기 요청 (보드 %d칸, 이동 %d회)"),
		*GetNameSafe(PlayerController->PlayerState), Board.Num(), MoveCount);

	PlayerController->Client_OpenSlidePuzzleUI(this);
}

void ADRS2SlidePuzzle::ReleaseControl(APlayerController* PlayerController)
{
	if (!HasAuthority() || !PlayerController) return;

	// 점유자가 아닌 사람의 반납은 무시한다 (위젯이 RequestClose 와 NativeDestruct 에서
	// 두 번 보내도 안전해야 하고, 남의 점유를 뺏는 경로가 되어서도 안 된다).
	if (Occupant && Occupant != PlayerController->PlayerState) return;

	SetOccupant(nullptr);
}

bool ADRS2SlidePuzzle::IsOccupiedByOther(const APlayerController* PlayerController) const
{
	if (!Occupant) return false;
	if (!PlayerController) return true;

	return Occupant != PlayerController->PlayerState;
}

void ADRS2SlidePuzzle::SetOccupant(APlayerState* NewOccupant)
{
	if (Occupant == NewOccupant) return;

	Occupant = NewOccupant;

	// 서버에서는 RepNotify 가 저절로 불리지 않는다.
	OnRep_Occupant();
}

bool ADRS2SlidePuzzle::HasControl(const APlayerController* PlayerController) const
{
	return Occupant != nullptr && PlayerController != nullptr && Occupant == PlayerController->PlayerState;
}

// ================= 조작 (서버 판정) =================

bool ADRS2SlidePuzzle::ApplyMoveInternal(int32 TileId)
{
	const int32 From = Board.IndexOfByKey(TileId);
	const int32 To   = Board.IndexOfByKey(0);   // 빈칸

	// 빈칸과 상하좌우로 맞닿아 있는가. 이것이 8퍼즐의 유일한 이동 조건이자
	// 클라 조작을 걸러내는 유일한 검문소다.
	if (From == INDEX_NONE || To == INDEX_NONE || !DRS2Puzzle::AreAdjacent(From, To))
	{
		return false;
	}

	// 조각이 빈칸으로 가고, 빈칸은 조각이 있던 자리로 온다.
	Swap(Board[From], Board[To]);
	return true;
}

bool ADRS2SlidePuzzle::TryMove(APlayerController* PlayerController, int32 TileId)
{
	if (!HasAuthority() || bSolved) return false;
	if (!HasControl(PlayerController)) return false;

	if (!ApplyMoveInternal(TileId)) return false;

	History.Add(TileId);
	MoveCount = History.Num();

	OnRep_Board();

	if (DRS2Puzzle::IsSolvedBoard(Board))
	{
		// 클라의 "풀었다" 보고를 기다리지 않는다. 판이 여기 있으므로 여기서 판정한다.
		NotifySolved();
	}
	return true;
}

bool ADRS2SlidePuzzle::TryUndo(APlayerController* PlayerController)
{
	if (!HasAuthority() || bSolved) return false;
	if (!HasControl(PlayerController)) return false;
	if (History.Num() == 0) return false;

	// 같은 조각을 한 번 더 움직이면 정확히 제자리로 돌아온다. 좌표를 저장할 필요가 없는 이유다.
	const int32 TileId = History.Last();

	// ★되돌리기는 히스토리에 기록하지 않는다★ 기록하면 Undo 가 서로를 되돌리며 제자리걸음을 한다.
	if (!ApplyMoveInternal(TileId)) return false;

	History.Pop();
	MoveCount = History.Num();

	OnRep_Board();
	return true;
}

bool ADRS2SlidePuzzle::TryReset(APlayerController* PlayerController)
{
	if (!HasAuthority() || bSolved) return false;
	if (!HasControl(PlayerController)) return false;
	if (StartBoard.Num() != DRS2Puzzle::CellCount) return false;

	// 새로 셔플하지 않는다. 꼬였을 때 원점으로 돌아가는 것이 목적이므로 새 배치를 주면 배신감이 든다.
	// ★단, 이 판은 팀 공용이라 남이 쌓아 둔 진행도까지 함께 날아간다★
	Board = StartBoard;
	History.Empty();
	MoveCount = 0;

	OnRep_Board();
	return true;
}

// ================= 해결 =================

void ADRS2SlidePuzzle::NotifySolved()
{
	if (!HasAuthority() || bSolved) return;

	bSolved = true;

	// 디버그 경로로 들어와도 화면이 정답 배치가 되도록 맞춰 둔다 (정상 경로에서는 이미 같다).
	Board = DRS2Puzzle::MakeSolvedBoard();

	// 이 시점에만 정답 숫자가 복제된다 (해결 전에는 클라에 정답 정보가 없다 - Plan6 §14.2.6)
	RevealedDigit = SecretDigit;

	// 더 조작할 것이 없다. 점유를 풀어 두면 여러 명이 동시에 번호를 확인할 수 있다.
	SetOccupant(nullptr);

	OnRep_Board();
	OnRep_RevealedDigit();

	UE_LOG(LogDR, Log, TEXT("[S2Puzzle] 8퍼즐 해결 - 첫 자리 공개 (이동 %d회)"), MoveCount);

	OnPuzzleSolved.Broadcast(this);
}

void ADRS2SlidePuzzle::DebugSolveInstantly()
{
	if (!HasAuthority())
	{
		UE_LOG(LogDR, Warning, TEXT("[S2Puzzle] DebugSolveInstantly 는 서버에서만 동작한다"));
		return;
	}

	History.Empty();
	MoveCount = 0;

	NotifySolved();
}

// ================= RepNotify =================

void ADRS2SlidePuzzle::OnRep_Board()
{
	// 열려 있는 위젯이 여기에 붙어 있다 (UDRS2SlidePuzzleWidget::BindToPuzzle).
	OnBoardChanged.Broadcast(Board, MoveCount);
}

void ADRS2SlidePuzzle::OnRep_Occupant()
{
	OnOccupantChanged.Broadcast(Occupant);
}

void ADRS2SlidePuzzle::OnRep_RevealedDigit()
{
	if (RevealedDigit >= 0)
	{
		OnDigitRevealed.Broadcast(RevealedDigit);
		OnPuzzleSolvedVisual(RevealedDigit);
	}
}
