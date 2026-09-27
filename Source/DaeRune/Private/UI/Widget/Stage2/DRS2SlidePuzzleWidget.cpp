// Copyright DaeRune

#include "UI/Widget/Stage2/DRS2SlidePuzzleWidget.h"

#include "Actor/Stage2/DRS2SlidePuzzle.h"
#include "Animation/WidgetAnimation.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Player/DRPlayerController.h"
#include "TimerManager.h"
#include "DaeRune/DRLogChannels.h"

// ================= 생명주기 =================

void UDRS2SlidePuzzleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// ★여기서 켜야 한다★ - bIsFocusable 은 Slate 위젯을 만들 때 한 번 읽히고 끝이다.
	// NativeConstruct 에서 켜면 이미 만들어진 뒤라 ESC / Ctrl+Z 가 들어오지 않는다.
	SetIsFocusable(true);
}

void UDRS2SlidePuzzleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Undo)
	{
		Btn_Undo->OnClicked.AddUniqueDynamic(this, &UDRS2SlidePuzzleWidget::HandleUndoClicked);
	}
	if (Btn_Reset)
	{
		Btn_Reset->OnClicked.AddUniqueDynamic(this, &UDRS2SlidePuzzleWidget::HandleResetClicked);
	}

	BuildTiles();

	// PC 가 Add to Viewport 보다 먼저 BindToPuzzle 을 부르므로, 그때는 조각이 아직 없다.
	// 조각이 준비된 지금 다시 한 번 맞춘다 (판 상태는 서버에 있으므로 언제 그려도 같은 결과다).
	SyncFromPuzzle();

	if (Anim_Intro)
	{
		PlayAnimation(Anim_Intro);
	}

	// 키 입력을 받으려면 포커스가 있어야 한다. 위젯을 띄우는 쪽에서
	// FInputModeUIOnly::SetWidgetToFocus 를 넣어 줬다면 중복이지만 해가 없다.
	SetKeyboardFocus();
}

void UDRS2SlidePuzzleWidget::NativeDestruct()
{
	// ★창이 사라지는 모든 경로에서 점유를 반납한다★
	// RequestClose 를 거치지 않는 경로(레벨 전환, HUD 파괴, 사망 처리)가 있고,
	// 여기서 놓치면 "아무도 못 여는 단말"이 된다. 서버는 점유자가 아닌 반납을 무시하므로
	// RequestClose 와 여기서 두 번 와도 안전하다.
	ReleaseControlOnServer();

	// RequestClose 를 거치지 않고 사라지는 경로(레벨 전환, HUD 파괴, 사망 처리)에서도
	// 입력 모드가 UI 에 묶인 채 남지 않게 한다. 이미 닫혔으면 PC 쪽 가드가 무시한다.
	CloseScreenOnController();

	ClearAckWait();

	if (ADRS2SlidePuzzle* Puzzle = OwnerPuzzle.Get())
	{
		Puzzle->OnBoardChanged.RemoveDynamic(this, &UDRS2SlidePuzzleWidget::HandleBoardChanged);
		Puzzle->OnDigitRevealed.RemoveDynamic(this, &UDRS2SlidePuzzleWidget::HandleDigitRevealed);
		Puzzle->OnOccupantChanged.RemoveDynamic(this, &UDRS2SlidePuzzleWidget::HandleOccupantChanged);
	}

	for (TPair<int32, TObjectPtr<UDRS2PuzzleTileWidget>>& Pair : TileWidgets)
	{
		if (Pair.Value)
		{
			Pair.Value->StopMotion();
		}
	}
	TileWidgets.Empty();

	// 다음에 열릴 때는 조각을 새로 만들고 처음부터 찍는다 (애니메이션 없이).
	DisplayBoard.Reset();
	bBoardDrawnOnce = false;
	PendingSlides   = 0;

	Super::NativeDestruct();
}

// ================= 액터와의 연결 =================

void UDRS2SlidePuzzleWidget::BindToPuzzle(ADRS2SlidePuzzle* InPuzzle)
{
	if (!IsValid(InPuzzle))
	{
		UE_LOG(LogDR, Error, TEXT("[S2Puzzle] BindToPuzzle 에 유효하지 않은 액터가 들어왔다"));
		return;
	}

	OwnerPuzzle = InPuzzle;

	// ★AddUnique 여야 한다★ - HUD 가 위젯을 재사용하면 열 때마다 여기로 들어온다.
	// 중복 바인딩되면 이동 한 번에 ApplyServerBoard 가 두 번 돌아 조각이 튄다.
	InPuzzle->OnBoardChanged.AddUniqueDynamic(this, &UDRS2SlidePuzzleWidget::HandleBoardChanged);
	InPuzzle->OnDigitRevealed.AddUniqueDynamic(this, &UDRS2SlidePuzzleWidget::HandleDigitRevealed);
	InPuzzle->OnOccupantChanged.AddUniqueDynamic(this, &UDRS2SlidePuzzleWidget::HandleOccupantChanged);

	SyncFromPuzzle();
}

void UDRS2SlidePuzzleWidget::SyncFromPuzzle()
{
	ADRS2SlidePuzzle* Puzzle = OwnerPuzzle.Get();
	if (!Puzzle || TileWidgets.Num() == 0)
	{
		// 조각이 아직 없다 (Add to Viewport 전). NativeConstruct 가 다시 부른다.
		return;
	}

	ApplyServerBoard(Puzzle->GetBoard());
	ShowCode(Puzzle->GetRevealedDigit());   // 이미 풀린 판이면 번호가 바로 뜬다 (-1 이면 표시 없음)
}

void UDRS2SlidePuzzleWidget::HandleBoardChanged(const TArray<int32>& NewBoard, int32 /*NewMoveCount*/)
{
	ApplyServerBoard(NewBoard);
}

void UDRS2SlidePuzzleWidget::HandleDigitRevealed(int32 Digit)
{
	ShowCode(Digit);
}

void UDRS2SlidePuzzleWidget::HandleOccupantChanged(APlayerState* NewOccupant)
{
	// 점유가 늦게 도착해도 이 시점에 잠금이 풀린다
	// (Client_OpenSlidePuzzleUI RPC 와 Occupant 복제는 도착 순서가 보장되지 않는다).
	UpdateInteractivity();

	OnOccupantChanged(NewOccupant);
}

// ================= 조각 생성 =================

void UDRS2SlidePuzzleWidget::BuildTiles()
{
	if (!CanvasPanel_Tiles)
	{
		UE_LOG(LogDR, Error, TEXT("[S2Puzzle] CanvasPanel_Tiles 가 없다. WBP 하이어라키를 확인할 것"));
		return;
	}
	if (!TileWidgetClass)
	{
		UE_LOG(LogDR, Error, TEXT("[S2Puzzle] TileWidgetClass 미지정. Class Defaults 에 WBP_S2PuzzleTile 을 넣을 것"));
		return;
	}

	CanvasPanel_Tiles->ClearChildren();
	TileWidgets.Empty();

	const int32 Count = FMath::Min(TileTextures.Num(), DRS2Puzzle::TileCount);
	if (TileTextures.Num() != DRS2Puzzle::TileCount)
	{
		UE_LOG(LogDR, Warning, TEXT("[S2Puzzle] TileTextures 가 %d장이다 (%d장 필요). 앞쪽 %d장만 쓴다"),
			TileTextures.Num(), DRS2Puzzle::TileCount, Count);
	}

	for (int32 i = 0; i < Count; ++i)
	{
		UDRS2PuzzleTileWidget* Tile = CreateWidget<UDRS2PuzzleTileWidget>(this, TileWidgetClass);
		if (!Tile)
		{
			continue;
		}

		UCanvasPanelSlot* CanvasSlot = CanvasPanel_Tiles->AddChildToCanvas(Tile);
		if (!CanvasSlot)
		{
			continue;
		}

		// ★네 줄 전부 필요하다★ - 하나라도 빠지면 조각이 캔버스 가운데로 몰리거나
		// AutoSize 상태로 붙어 칸 크기와 어긋난다.
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		CanvasSlot->SetAlignment(FVector2D::ZeroVector);
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetSize(FVector2D(DRS2Puzzle::CellSize, DRS2Puzzle::CellSize));

		const int32 TileId = i + 1;
		Tile->InitTile(TileId, TileTextures[i]);
		Tile->OnTileClicked.BindUObject(this, &UDRS2SlidePuzzleWidget::HandleTileClicked);
		Tile->OnSlideFinished.BindUObject(this, &UDRS2SlidePuzzleWidget::HandleSlideFinished);

		TileWidgets.Add(TileId, Tile);
	}

	OnBoardBuilt();
}

// ================= 서버 보드 -> 화면 =================

void UDRS2SlidePuzzleWidget::ApplyServerBoard(const TArray<int32>& NewBoard)
{
	if (NewBoard.Num() != DRS2Puzzle::CellCount)
	{
		// 서버가 아직 판을 안 만들었다 (BeginPlay 이전에 열렸을 때).
		return;
	}
	if (TileWidgets.Num() == 0)
	{
		return;
	}

	// 다른 칸을 모은다. 이 개수 하나가 연출을 고른다.
	TArray<int32> Diff;
	for (int32 i = 0; i < DRS2Puzzle::CellCount; ++i)
	{
		if (!DisplayBoard.IsValidIndex(i) || DisplayBoard[i] != NewBoard[i])
		{
			Diff.Add(i);
		}
	}

	DisplayBoard = NewBoard;
	EmptyIndex   = NewBoard.IndexOfByKey(0);

	ClearAckWait();

	const bool bFirstDraw = !bBoardDrawnOnce;

	if (Diff.Num() == 2 && !bFirstDraw)
	{
		// 한 칸 이동. 두 칸 중 새 보드에서 0 이 아닌 쪽이 조각의 새 자리다.
		const int32 To     = (NewBoard[Diff[0]] != 0) ? Diff[0] : Diff[1];
		const int32 From   = (To == Diff[0]) ? Diff[1] : Diff[0];
		const int32 TileId = NewBoard[To];

		PendingSlides = 1;
		if (TObjectPtr<UDRS2PuzzleTileWidget>* Found = TileWidgets.Find(TileId))
		{
			(*Found)->SlideToIndex(To, MoveDuration);
		}
		else
		{
			PendingSlides = 0;   // 위젯이 없으면 영영 콜백이 안 온다
		}

		OnMoveApplied(TileId, From, To);
	}
	else if (Diff.Num() > 0)
	{
		// Reset · 첫 오픈 · 어긋남 복구. 여러 조각이 한꺼번에 움직인다.
		// ★두 번 돌아야 한다★ - 먼저 전부 세고, 그 다음에 이동시킨다.
		// 한 번에 하면 첫 조각의 콜백이 즉시 돌아와 카운터가 1에서 0으로 떨어지고
		// 나머지가 출발하기도 전에 잠금이 풀린다.
		PendingSlides = 0;

		if (!bFirstDraw)
		{
			for (int32 i = 0; i < NewBoard.Num(); ++i)
			{
				if (NewBoard[i] != 0 && TileWidgets.Contains(NewBoard[i]))
				{
					++PendingSlides;
				}
			}
		}

		for (int32 i = 0; i < NewBoard.Num(); ++i)
		{
			const int32 TileId = NewBoard[i];
			if (TileId == 0)
			{
				continue;
			}
			if (TObjectPtr<UDRS2PuzzleTileWidget>* Found = TileWidgets.Find(TileId))
			{
				if (bFirstDraw)
				{
					// SnapToIndex 는 완료 델리게이트를 쏘지 않는다 (PendingSlides 를 오염시키지 않는다).
					(*Found)->SnapToIndex(i);
				}
				else
				{
					(*Found)->SlideToIndex(i, ResetDuration);
				}
			}
		}
	}

	bBoardDrawnOnce = true;

	UpdateInteractivity();

	ADRS2SlidePuzzle* Puzzle = OwnerPuzzle.Get();

	if (bFirstDraw && Puzzle && Puzzle->GetMoveCount() > 0)
	{
		UE_LOG(LogDR, Log, TEXT("[S2Puzzle] 진행 중인 공유 판을 연다 (이동 %d회)"), Puzzle->GetMoveCount());

		OnBoardResumed(Puzzle->GetMoveCount());
	}

	// 서버가 이미 풀렸다고 했다. 뒤늦게 연 사람(번호 확인용)도 여기로 들어온다.
	if (!bSolvedOnce && Puzzle && Puzzle->IsSolved())
	{
		HandleSolved();
	}
}

// ================= 클릭 -> 이동 요청 =================

void UDRS2SlidePuzzleWidget::HandleTileClicked(int32 InTileId)
{
	if (bInputLocked || bSolvedOnce)
	{
		return;
	}

	ADRS2SlidePuzzle* Puzzle = OwnerPuzzle.Get();
	if (!Puzzle)
	{
		// 액터 없이는 한 칸도 못 움직인다. HUD 의 BindToPuzzle 누락이 거의 유일한 원인이다.
		UE_LOG(LogDR, Warning, TEXT("[S2Puzzle] OwnerPuzzle 이 없다. HUD 에서 BindToPuzzle 을 불렀는지 확인할 것"));
		return;
	}

	const int32 From = DisplayBoard.IndexOfByKey(InTileId);
	if (From == INDEX_NONE)
	{
		return;
	}

	// 빈칸과 상하좌우로 맞닿아 있는가.
	// ★이 검사는 '연출용'이다★ - 못 가는 조각을 서버 왕복 없이 즉시 흔들어 주려고 한 번 본다.
	//   통과해도 이동이 확정된 것은 아니다. 확정은 서버의 TryMove 가 한다.
	if (!DRS2Puzzle::AreAdjacent(From, EmptyIndex))
	{
		if (TObjectPtr<UDRS2PuzzleTileWidget>* Found = TileWidgets.Find(InTileId))
		{
			(*Found)->PlayInvalidShake();
		}
		OnInvalidMove(InTileId);
		return;
	}

	if (TObjectPtr<UDRS2PuzzleTileWidget>* Found = TileWidgets.Find(InTileId))
	{
		// 눌림 연출은 즉시 재생한다 (왕복 동안 화면이 죽어 있지 않도록). 흔들림과는 배타다.
		(*Found)->PlayPressFeedback();
	}

	BeginAckWait();

	if (ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwningPlayer()))
	{
		PlayerController->Server_SlidePuzzleMove(Puzzle, InTileId);
	}
}

void UDRS2SlidePuzzleWidget::HandleSlideFinished(UDRS2PuzzleTileWidget* /*Tile*/)
{
	// 어디선가 흘러들어온 콜백 방어 (카운터가 음수로 내려가면 잠금이 영영 안 풀린다).
	if (PendingSlides <= 0)
	{
		return;
	}

	if (--PendingSlides > 0)
	{
		return;   // Reset 은 8개가 끝나야 0 이 된다
	}

	UpdateInteractivity();
}

void UDRS2SlidePuzzleWidget::HandleSolved()
{
	bSolvedOnce = true;

	// 완성 후에는 영구 잠금이다. 다 맞춘 그림을 다시 흐트러뜨릴 이유가 없다.
	SetInputLocked(true);
	RefreshButtons();

	if (Anim_Solved)
	{
		PlayAnimation(Anim_Solved);
	}

	UE_LOG(LogDR, Log, TEXT("[S2Puzzle] 8퍼즐 완성 화면 처리"));

	OnSolvedVisual();
	OnPuzzleSolved.Broadcast();
}

// ================= Undo / Reset / Close =================

void UDRS2SlidePuzzleWidget::RequestUndo()
{
	if (bInputLocked || !HasControl() || GetMoveCount() == 0)
	{
		return;
	}

	BeginAckWait();

	if (ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwningPlayer()))
	{
		PlayerController->Server_SlidePuzzleUndo(OwnerPuzzle.Get());
	}
}

void UDRS2SlidePuzzleWidget::RequestReset()
{
	if (bInputLocked || !HasControl())
	{
		return;
	}

	BeginAckWait();

	if (ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwningPlayer()))
	{
		PlayerController->Server_SlidePuzzleReset(OwnerPuzzle.Get());
	}
}

void UDRS2SlidePuzzleWidget::RequestClose()
{
	ReleaseControlOnServer();

	// ★창을 내리는 주체는 PlayerController 다★
	// 이 프로젝트의 화면 UI 는 전부 PC 가 소유한다 (대기실 · 게임오버 · 업그레이드 · 옷장).
	// 위젯 제거도, 입력 모드 복귀(RestoreDefaultInputMode 의 레벨별 분기)도 저쪽 책임이다.
	if (!CloseScreenOnController())
	{
		RemoveFromParent();   // PC 를 못 찾은 예외 경로에서만 스스로 빠진다
	}
}

bool UDRS2SlidePuzzleWidget::CloseScreenOnController()
{
	if (ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwningPlayer()))
	{
		PlayerController->CloseSlidePuzzleScreen();
		return true;
	}
	return false;
}

void UDRS2SlidePuzzleWidget::ReleaseControlOnServer()
{
	ADRS2SlidePuzzle* Puzzle = OwnerPuzzle.Get();
	if (!Puzzle)
	{
		return;
	}

	if (ADRPlayerController* PlayerController = Cast<ADRPlayerController>(GetOwningPlayer()))
	{
		PlayerController->Server_SlidePuzzleRelease(Puzzle);
	}
}

// ================= 서버 확정 대기 =================

void UDRS2SlidePuzzleWidget::BeginAckWait()
{
	bWaitingForAck = true;
	SetInputLocked(true);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(AckTimer, this, &UDRS2SlidePuzzleWidget::HandleAckTimeout, MoveAckTimeout, false);
	}
}

void UDRS2SlidePuzzleWidget::ClearAckWait()
{
	bWaitingForAck = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AckTimer);
	}
}

void UDRS2SlidePuzzleWidget::HandleAckTimeout()
{
	// 서버가 거절했거나 패킷이 늦다. 거절은 보드를 바꾸지 않으므로 복제도 오지 않는다
	// - 이 타이머가 없으면 여기서 입력이 영영 잠긴다.
	bWaitingForAck = false;

	if (ADRS2SlidePuzzle* Puzzle = OwnerPuzzle.Get())
	{
		ApplyServerBoard(Puzzle->GetBoard());   // 화면을 서버 보드로 다시 맞춘다
	}

	UpdateInteractivity();
}

// ================= 표시 =================

void UDRS2SlidePuzzleWidget::ShowCode(int32 Digit)
{
	if (!Text_Code)
	{
		if (Digit >= 0)
		{
			UE_LOG(LogDR, Warning, TEXT("[S2Puzzle] Text_Code 가 없어 금고 번호를 표시할 수 없다 (WBP 하이어라키 확인)"));
		}
		return;
	}

	if (Digit < 0)
	{
		Text_Code->SetText(FText::GetEmpty());
		Text_Code->SetRenderOpacity(0.f);
		return;
	}

	Text_Code->SetText(FText::AsNumber(Digit));

	if (Anim_CodeReveal)
	{
		PlayAnimation(Anim_CodeReveal);
	}
	else
	{
		// 등장 애니메이션이 없으면 디자이너가 내려 둔 투명도를 직접 되돌린다.
		Text_Code->SetRenderOpacity(1.f);
	}

	OnCodeRevealed(Digit);
}

// ================= 상태/헬퍼 =================

void UDRS2SlidePuzzleWidget::SetInputLocked(bool bLocked)
{
	if (bInputLocked == bLocked)
	{
		return;
	}

	bInputLocked = bLocked;
	RefreshButtons();
	OnInputLockChanged(bLocked);
}

void UDRS2SlidePuzzleWidget::UpdateInteractivity()
{
	const bool bBusy = (PendingSlides > 0) || bWaitingForAck;

	SetInputLocked(bBusy || !HasControl());
	RefreshButtons();
}

void UDRS2SlidePuzzleWidget::RefreshButtons()
{
	const bool bInteractive = !bInputLocked && HasControl();

	if (Btn_Undo)
	{
		Btn_Undo->SetIsEnabled(bInteractive && GetMoveCount() > 0);
	}
	if (Btn_Reset)
	{
		Btn_Reset->SetIsEnabled(bInteractive);
	}
}

bool UDRS2SlidePuzzleWidget::IsSolved() const
{
	const ADRS2SlidePuzzle* Puzzle = OwnerPuzzle.Get();
	return Puzzle ? Puzzle->IsSolved() : false;
}

int32 UDRS2SlidePuzzleWidget::GetMoveCount() const
{
	const ADRS2SlidePuzzle* Puzzle = OwnerPuzzle.Get();
	return Puzzle ? Puzzle->GetMoveCount() : 0;
}

FText UDRS2SlidePuzzleWidget::GetOccupantName() const
{
	const ADRS2SlidePuzzle* Puzzle = OwnerPuzzle.Get();
	if (!Puzzle)
	{
		return FText::GetEmpty();
	}

	const APlayerState* Occupant = Puzzle->GetOccupant();
	return Occupant ? FText::FromString(Occupant->GetPlayerName()) : FText::GetEmpty();
}

bool UDRS2SlidePuzzleWidget::HasControl() const
{
	const ADRS2SlidePuzzle* Puzzle = OwnerPuzzle.Get();
	if (!Puzzle || Puzzle->IsSolved())
	{
		return false;   // 해결된 판은 번호 확인용 읽기 전용이다
	}

	// ★Occupant 가 아직 안 내려왔을 수 있다★
	// Client_OpenSlidePuzzleUI(RPC) 와 Occupant(프로퍼티 복제) 는 서로 다른 채널이라
	// 도착 순서가 보장되지 않는다. 그 잠깐을 잠가 두면 첫 클릭이 씹히므로 '비어 있으면 내 것'으로 본다.
	// 틀려도 서버가 거절할 뿐이라 손해가 없고, 점유가 도착하면 OnOccupantChanged 가 바로잡는다.
	const APlayerState* Occupant = Puzzle->GetOccupant();
	return Occupant == nullptr || Occupant == GetOwningPlayerState();
}

// ================= 입력 =================

void UDRS2SlidePuzzleWidget::HandleUndoClicked()
{
	RequestUndo();
}

void UDRS2SlidePuzzleWidget::HandleResetClicked()
{
	RequestReset();
}

FReply UDRS2SlidePuzzleWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bHandleKeyboardShortcuts)
	{
		const FKey Key = InKeyEvent.GetKey();

		if (Key == EKeys::Escape)
		{
			RequestClose();
			return FReply::Handled();
		}
		if (Key == EKeys::Z && InKeyEvent.IsControlDown())
		{
			RequestUndo();
			return FReply::Handled();
		}
		if (Key == EKeys::R)
		{
			RequestReset();
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
