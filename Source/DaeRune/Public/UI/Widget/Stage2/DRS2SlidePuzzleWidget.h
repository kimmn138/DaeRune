// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "UI/Widget/Stage2/DRS2PuzzleTileWidget.h"
#include "DRS2SlidePuzzleWidget.generated.h"

class UButton;
class UCanvasPanel;
class UTextBlock;

// 퍼즐이 풀렸다. ★정답 숫자를 싣지 않는다★ - 숫자는 서버가 복제해 준 값만 쓴다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRS2OnSlidePuzzleSolved);

/**
 * 8퍼즐 보드 전체. WBP_S2SlidePuzzle 의 C++ 베이스.
 *
 * 이 클래스가 책임지는 것은 네 가지뿐이다.
 *   1) 보드 상태(Board / EmptyIndex / History)와 규칙 판정
 *   2) 조각 위젯 8개 생성과 Canvas Slot 배치
 *   3) 클릭 -> 이동 가능 판정 -> 미끄러짐 또는 흔들림 지시
 *   4) 보간 중 입력 잠금
 *
 * 그리기, 사운드, 등장/완성 연출, 서버 보고는 전부 BP(WBP_S2SlidePuzzle) 몫이다.
 * BP 는 아래 BlueprintImplementableEvent 훅과 OnPuzzleSolved 델리게이트로만 끼어든다.
 *
 * ★정답 숫자를 이 위젯이 들고 있지 않는 이유★
 * ADRS2SlidePuzzle 은 "해결 전에는 클라에 정답 정보가 존재하지 않는다"를 계약으로 잡고 있다
 * (SecretDigit 는 서버 전용, RevealedDigit 만 복제). 위젯 Class Defaults 에 숫자를 박으면
 * 그 계약이 무너진다 - 쿠킹된 위젯 에셋을 열면 누구나 금고 번호를 읽을 수 있다.
 * 그래서 이 위젯은 숫자를 '받아서 표시만' 한다 (ShowCode).
 */
UCLASS(Abstract)
class DAERUNE_API UDRS2SlidePuzzleWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	/** 퍼즐이 풀린 순간. BP 가 여기에 붙어 서버 보고(NotifySolved 경로)를 수행한다. */
	UPROPERTY(BlueprintAssignable, Category = "S2|Puzzle")
	FDRS2OnSlidePuzzleSolved OnPuzzleSolved;

	/** 새 판을 만든다 (셔플 -> 즉시 배치 -> 히스토리 비움). */
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	void StartNewPuzzle();

	/** 마지막 이동 되돌리기. 히스토리가 비었거나 입력 잠금 중이면 아무 일도 없다. */
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	void RequestUndo();

	/** 그 판의 시작 배치로 복귀 (새 배치를 주지 않는다). */
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	void RequestReset();

	/** 입력 모드를 게임으로 되돌리고 위젯을 제거한다. */
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	void RequestClose();

	/**
	 * 금고 번호 한 자리를 표시한다. 서버가 RevealedDigit 를 복제한 뒤 BP 가 호출한다.
	 * 음수를 주면 표시를 지운다.
	 */
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	void ShowCode(int32 Digit);

	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	bool IsSolved() const;

	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	bool IsInputLocked() const { return bInputLocked; }

	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	int32 GetMoveCount() const { return History.Num(); }

	/** 테스트 전용. 보드를 정답 배치로 밀어넣고 해결 처리까지 태운다. */
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle|Debug", meta = (DevelopmentOnly))
	void DebugSolveInstantly();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// ===== BindWidget =====

	/** 조각 8개가 붙을 캔버스. 디자이너에서는 비어 있다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CanvasPanel_Tiles;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Code;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Undo;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Reset;

	// ===== BindWidgetAnim (전부 선택) =====

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Intro;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Solved;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_CodeReveal;

	// ===== 튜닝값 =====

	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle")
	TSubclassOf<UDRS2PuzzleTileWidget> TileWidgetClass;

	/** Puzzle01 ~ Puzzle08 순서대로 넣는다. 이 순서가 곧 정답 순서다. */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle")
	TArray<TObjectPtr<UTexture2D>> TileTextures;

	/** 정답에서 출발해 합법 이동을 몇 번 무작위로 밟을지. 20 미만이면 눈에 띄게 쉬워진다. */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle", meta = (ClampMin = "20", ClampMax = "500"))
	int32 ShuffleSteps = 60;

	/** 한 칸 이동 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle", meta = (ClampMin = "0.02", ClampMax = "1.0"))
	float MoveDuration = 0.16f;

	/** Reset 시 8개가 동시에 움직이는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle", meta = (ClampMin = "0.02", ClampMax = "2.0"))
	float ResetDuration = 0.22f;

	/** 열리자마자 새 판을 시작할지. 끄면 BP 가 원하는 시점에 StartNewPuzzle 을 부른다. */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle")
	bool bStartOnConstruct = true;

	/** ESC 닫기 / Ctrl+Z 되돌리기 / R 초기화 단축키 사용 여부. */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle")
	bool bHandleKeyboardShortcuts = true;

	// ===== BP 연출 훅 =====

	/** 조각 8개 생성이 끝났다. BP 가 등장 연출을 여기서 시작한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnBoardBuilt();

	/** 이동이 확정됐다 (보간은 지금부터). 사운드/카운터 갱신용. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnMoveApplied(int32 TileId, int32 FromIndex, int32 ToIndex);

	/** 갈 수 없는 조각을 눌렀다. 조각은 이미 흔들리고 있다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnInvalidMove(int32 TileId);

	/** 퍼즐이 풀렸다 (Anim_Solved 재생 직후). 숫자 공개는 ShowCode 로 따로 들어온다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnSolvedVisual();

	/** 입력 잠금 상태가 바뀌었다. 커서/버튼 외의 추가 연출용. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnInputLockChanged(bool bLocked);

	UFUNCTION()
	void HandleUndoClicked();

	UFUNCTION()
	void HandleResetClicked();

private:
	// ===== 보드 규칙 =====

	/** 정답 배치에서 합법 이동을 ShuffleSteps 번 밟아 '반드시 풀리는' 배치를 만든다. */
	void Shuffle();

	/** Board 내용대로 조각 8개를 애니메이션 없이 제자리에 찍는다. */
	void ApplyBoardInstant();

	/** 조각 위젯 8개 생성 + Canvas Slot 세팅. NativeConstruct 에서 1회. */
	void BuildTiles();

	/**
	 * 실제 이동. 인접하지 않으면 false 를 돌려주고 보드를 건드리지 않는다.
	 * bRecordHistory 는 Undo 경로에서만 false 다 (되돌리기를 또 기록하면 무한루프가 된다).
	 */
	bool MoveTile(int32 InTileId, float Duration, bool bRecordHistory);

	void HandleTileClicked(int32 InTileId);
	void HandleSlideFinished(UDRS2PuzzleTileWidget* Tile);
	void HandleSolved();

	void SetInputLocked(bool bLocked);
	void RefreshButtons();

	/** 상하좌우로 맞닿은 칸 인덱스들. 줄이 넘어가는 이동은 여기서 걸러진다. */
	TArray<int32> GetNeighbors(int32 Index) const;

	/** 맨해튼 거리 1 검사. Abs(A-B)==1 로 하면 인덱스 2<->3 이 통과해 버린다. */
	static bool AreAdjacent(int32 A, int32 B);

	// ===== 상태 =====

	/** 길이 9. 값 = TileId(1~8), 0 = 빈칸. 인덱스 = Row * GridSize + Col */
	TArray<int32> Board;

	/** 이번 판의 시작 배치. Reset 이 이걸 되돌린다. */
	TArray<int32> StartBoard;

	/** 움직인 TileId 순서. 같은 조각을 한 번 더 움직이면 제자리로 오므로 좌표는 필요 없다. */
	TArray<int32> History;

	int32 EmptyIndex = DRS2Puzzle::CellCount - 1;

	UPROPERTY()
	TMap<int32, TObjectPtr<UDRS2PuzzleTileWidget>> TileWidgets;

	bool bInputLocked = false;
	bool bSolvedOnce  = false;

	/** 아직 도착하지 않은 조각 수. 0 이 될 때만 잠금을 푼다 (Reset 은 8개가 동시에 끝난다). */
	int32 PendingSlides = 0;
};
