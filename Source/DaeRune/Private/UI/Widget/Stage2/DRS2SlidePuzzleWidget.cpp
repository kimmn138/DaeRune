// Copyright DaeRune

#include "UI/Widget/Stage2/DRS2SlidePuzzleWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
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

	if (bStartOnConstruct)
	{
		StartNewPuzzle();
	}

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
	for (TPair<int32, TObjectPtr<UDRS2PuzzleTileWidget>>& Pair : TileWidgets)
	{
		if (Pair.Value)
		{
			Pair.Value->StopMotion();
		}
	}
	TileWidgets.Empty();

	Super::NativeDestruct();
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

// ================= 판 진행 =================

void UDRS2SlidePuzzleWidget::StartNewPuzzle()
{
	Shuffle();

	StartBoard = Board;
	History.Empty();
	PendingSlides = 0;
	bSolvedOnce   = false;
	bInputLocked  = false;

	ApplyBoardInstant();

	if (Text_Code)
	{
		Text_Code->SetText(FText::GetEmpty());
		Text_Code->SetRenderOpacity(0.f);
	}

	RefreshButtons();
}

void UDRS2SlidePuzzleWidget::Shuffle()
{
	// ★무작위 순열을 쓰면 안 된다★ - 9칸을 그냥 섞으면 절반은 아무리 움직여도 안 풀린다
	// (순열의 짝홀이 정답과 달라진다). 정답에서 출발해 합법 이동만 밟으면 항상 되짚어 갈 수 있다.
	constexpr int32 MaxAttempts = 8;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		Board.Reset(DRS2Puzzle::CellCount);
		for (int32 i = 1; i < DRS2Puzzle::CellCount; ++i)
		{
			Board.Add(i);
		}
		Board.Add(0);

		int32 Empty     = DRS2Puzzle::CellCount - 1;
		int32 PrevEmpty = INDEX_NONE;

		for (int32 Step = 0; Step < ShuffleSteps; ++Step)
		{
			TArray<int32> Candidates = GetNeighbors(Empty);

			// 직전에 빈칸이 있던 자리로 되돌아가면 방금 한 이동이 취소된다.
			Candidates.Remove(PrevEmpty);
			if (Candidates.Num() == 0)
			{
				continue;
			}

			const int32 Pick = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
			Swap(Board[Empty], Board[Pick]);

			PrevEmpty = Empty;
			Empty     = Pick;
		}

		EmptyIndex = Empty;

		// 드물게 제자리로 돌아온다. 그 판은 버리고 다시 섞는다.
		if (!IsSolved())
		{
			return;
		}
	}

	UE_LOG(LogDR, Warning, TEXT("[S2Puzzle] 셔플이 %d회 연속 정답 배치로 끝났다. ShuffleSteps(%d) 확인 필요"),
		MaxAttempts, ShuffleSteps);
}

void UDRS2SlidePuzzleWidget::ApplyBoardInstant()
{
	for (int32 i = 0; i < Board.Num(); ++i)
	{
		const int32 TileId = Board[i];
		if (TileId == 0)
		{
			continue;
		}
		if (TObjectPtr<UDRS2PuzzleTileWidget>* Found = TileWidgets.Find(TileId))
		{
			// SnapToIndex 는 완료 델리게이트를 쏘지 않는다 (PendingSlides 를 오염시키지 않기 위해).
			(*Found)->SnapToIndex(i);
		}
	}
}

// ================= 클릭 -> 판정 -> 이동 =================

void UDRS2SlidePuzzleWidget::HandleTileClicked(int32 InTileId)
{
	if (bInputLocked || bSolvedOnce)
	{
		return;
	}

	const int32 From = Board.IndexOfByKey(InTileId);
	if (From == INDEX_NONE)
	{
		return;
	}

	// 빈칸과 상하좌우로 맞닿아 있는가. 이것이 8퍼즐의 유일한 이동 조건이다.
	if (!AreAdjacent(From, EmptyIndex))
	{
		if (TObjectPtr<UDRS2PuzzleTileWidget>* Found = TileWidgets.Find(InTileId))
		{
			(*Found)->PlayInvalidShake();
		}
		OnInvalidMove(InTileId);
		return;
	}

	MoveTile(InTileId, MoveDuration, /*bRecordHistory=*/true);
}

bool UDRS2SlidePuzzleWidget::MoveTile(int32 InTileId, float Duration, bool bRecordHistory)
{
	const int32 From = Board.IndexOfByKey(InTileId);
	if (From == INDEX_NONE || !AreAdjacent(From, EmptyIndex))
	{
		return false;
	}

	// 조각이 빈칸으로 가고, 빈칸은 조각이 있던 자리로 온다.
	const int32 To = EmptyIndex;
	Swap(Board[From], Board[To]);
	EmptyIndex = From;

	if (bRecordHistory)
	{
		History.Add(InTileId);
	}

	// ★잠금과 카운터를 SlideToIndex 보다 먼저 세운다★
	// Duration 이 0 이면 완료 콜백이 그 자리에서 되돌아오기 때문이다.
	SetInputLocked(true);
	PendingSlides = 1;

	if (TObjectPtr<UDRS2PuzzleTileWidget>* Found = TileWidgets.Find(InTileId))
	{
		(*Found)->PlayPressFeedback();
		(*Found)->SlideToIndex(To, Duration);
	}
	else
	{
		// 위젯이 없으면 영영 콜백이 안 온다. 잠금을 직접 푼다.
		PendingSlides = 0;
		SetInputLocked(false);
	}

	OnMoveApplied(InTileId, From, To);
	return true;
}

void UDRS2SlidePuzzleWidget::HandleSlideFinished(UDRS2PuzzleTileWidget* Tile)
{
	// 어디선가 흘러들어온 콜백 방어 (카운터가 음수로 내려가면 잠금이 영영 안 풀린다).
	if (PendingSlides <= 0)
	{
		return;
	}

	if (--PendingSlides > 0)
	{
		return;
	}

	if (!bSolvedOnce && IsSolved())
	{
		HandleSolved();
		return;
	}

	SetInputLocked(false);
	RefreshButtons();
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

	UE_LOG(LogDR, Log, TEXT("[S2Puzzle] 8퍼즐 완성 (이동 %d회)"), History.Num());

	OnSolvedVisual();
	OnPuzzleSolved.Broadcast();
}

// ================= Undo / Reset / Close =================

void UDRS2SlidePuzzleWidget::RequestUndo()
{
	if (bInputLocked || bSolvedOnce || History.Num() == 0)
	{
		return;
	}

	// 같은 조각을 한 번 더 움직이면 정확히 제자리로 돌아온다. 좌표를 저장할 필요가 없는 이유다.
	const int32 TileId = History.Last();

	// ★되돌리기는 히스토리에 기록하지 않는다★ 기록하면 Undo 가 서로를 되돌리며 제자리걸음을 한다.
	if (MoveTile(TileId, MoveDuration, /*bRecordHistory=*/false))
	{
		History.Pop();
	}
}

void UDRS2SlidePuzzleWidget::RequestReset()
{
	if (bInputLocked || bSolvedOnce)
	{
		return;
	}

	// 새로 셔플하지 않는다. 꼬였을 때 원점으로 돌아가는 것이 목적이므로 새 배치를 주면 배신감이 든다.
	Board      = StartBoard;
	EmptyIndex = Board.IndexOfByKey(0);
	History.Empty();

	SetInputLocked(true);

	// ★두 번 돌아야 한다★ - 먼저 전부 세고, 그 다음에 이동시킨다.
	// 한 번에 하면 첫 조각의 콜백이 (0초 이동일 때) 즉시 돌아와 카운터가 1에서 0으로 떨어지고
	// 나머지 7개가 출발하기도 전에 잠금이 풀린다.
	PendingSlides = 0;
	for (int32 i = 0; i < Board.Num(); ++i)
	{
		if (Board[i] != 0 && TileWidgets.Contains(Board[i]))
		{
			++PendingSlides;
		}
	}

	if (PendingSlides == 0)
	{
		SetInputLocked(false);
		RefreshButtons();
		return;
	}

	for (int32 i = 0; i < Board.Num(); ++i)
	{
		const int32 TileId = Board[i];
		if (TileId == 0)
		{
			continue;
		}
		if (TObjectPtr<UDRS2PuzzleTileWidget>* Found = TileWidgets.Find(TileId))
		{
			(*Found)->SlideToIndex(i, ResetDuration);
		}
	}
}

void UDRS2SlidePuzzleWidget::RequestClose()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->SetShowMouseCursor(false);
	}

	RemoveFromParent();
}

// ================= 표시 =================

void UDRS2SlidePuzzleWidget::ShowCode(int32 Digit)
{
	if (!Text_Code)
	{
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
		// 등장 애니메이션이 없으면 StartNewPuzzle 이 내려 둔 투명도를 직접 되돌린다.
		Text_Code->SetRenderOpacity(1.f);
	}
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

void UDRS2SlidePuzzleWidget::RefreshButtons()
{
	const bool bInteractive = !bInputLocked && !bSolvedOnce;

	if (Btn_Undo)
	{
		Btn_Undo->SetIsEnabled(bInteractive && History.Num() > 0);
	}
	if (Btn_Reset)
	{
		Btn_Reset->SetIsEnabled(bInteractive);
	}
}

bool UDRS2SlidePuzzleWidget::IsSolved() const
{
	if (Board.Num() != DRS2Puzzle::CellCount)
	{
		return false;
	}

	// 마지막 칸은 검사할 필요가 없다. 앞이 전부 맞으면 나머지는 자동으로 빈칸이다.
	for (int32 i = 0; i < DRS2Puzzle::CellCount - 1; ++i)
	{
		if (Board[i] != i + 1)
		{
			return false;
		}
	}
	return true;
}

TArray<int32> UDRS2SlidePuzzleWidget::GetNeighbors(int32 Index) const
{
	TArray<int32> Out;
	Out.Reserve(4);

	const int32 Row = Index / DRS2Puzzle::GridSize;
	const int32 Col = Index % DRS2Puzzle::GridSize;

	if (Row > 0)                        { Out.Add(Index - DRS2Puzzle::GridSize); }
	if (Row < DRS2Puzzle::GridSize - 1) { Out.Add(Index + DRS2Puzzle::GridSize); }
	if (Col > 0)                        { Out.Add(Index - 1); }
	if (Col < DRS2Puzzle::GridSize - 1) { Out.Add(Index + 1); }

	return Out;
}

bool UDRS2SlidePuzzleWidget::AreAdjacent(int32 A, int32 B)
{
	// ★Abs(A-B)==1 로 하면 안 된다★ - 인덱스 2와 3은 화면에서 줄이 다른데 값 차이는 1이다.
	// 오른쪽 끝 조각이 왼쪽 끝으로 순간이동하는 버그가 여기서 나온다.
	const int32 RowA = A / DRS2Puzzle::GridSize, ColA = A % DRS2Puzzle::GridSize;
	const int32 RowB = B / DRS2Puzzle::GridSize, ColB = B % DRS2Puzzle::GridSize;

	return FMath::Abs(RowA - RowB) + FMath::Abs(ColA - ColB) == 1;
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

// ================= 디버그 =================

void UDRS2SlidePuzzleWidget::DebugSolveInstantly()
{
	Board.Reset(DRS2Puzzle::CellCount);
	for (int32 i = 1; i < DRS2Puzzle::CellCount; ++i)
	{
		Board.Add(i);
	}
	Board.Add(0);

	EmptyIndex    = DRS2Puzzle::CellCount - 1;
	PendingSlides = 0;
	History.Empty();

	ApplyBoardInstant();

	bSolvedOnce = false;
	HandleSolved();
}
