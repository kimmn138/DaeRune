// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "UI/Widget/DRUserWidget.h"
#include "UI/Widget/Stage2/DRS2PuzzleTileWidget.h"
#include "DRS2SlidePuzzleWidget.generated.h"

class ADRS2SlidePuzzle;
class APlayerState;
class UButton;
class UCanvasPanel;
class UTextBlock;

// 퍼즐이 풀렸다. ★정답 숫자를 싣지 않는다★ - 숫자는 서버가 복제해 준 값만 쓴다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDRS2OnSlidePuzzleSolved);

/**
 * 8퍼즐 보드 전체. WBP_S2SlidePuzzle 의 C++ 베이스.
 *
 * ★2026-09-20 개정 - 이 위젯은 더 이상 판의 주인이 아니다★
 * "조작은 동시에 1명, 진행도는 팀 전체 공유" 요구에 따라 보드/히스토리/점유권이
 * 서버 액터(ADRS2SlidePuzzle)로 옮겨갔다. 이 위젯은 복제된 보드를 그리는 뷰다.
 *
 * 그래서 이 클래스가 책임지는 것은 네 가지다.
 *   1) 조각 위젯 8개 생성과 Canvas Slot 배치
 *   2) 서버 보드 -> 화면 반영 (ApplyServerBoard 하나가 이동·Undo·Reset·첫 오픈을 전부 처리한다)
 *   3) 클릭 -> 로컬 인접 검사(흔들림 연출용) -> 이동 요청 RPC
 *   4) 보간 중 / 서버 확정 대기 중 입력 잠금
 *
 * 그리기, 사운드, 등장/완성 연출은 BP(WBP_S2SlidePuzzle) 몫이다.
 * 반면 "서버에 풀었다고 보고"는 이제 BP 가 할 일이 아니다 - 서버가 직접 판정한다.
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
	/** 퍼즐이 풀린 순간 (서버 판정이 복제되어 도착한 시점). 바깥에서 듣고 싶을 때 쓴다. */
	UPROPERTY(BlueprintAssignable, Category = "S2|Puzzle")
	FDRS2OnSlidePuzzleSolved OnPuzzleSolved;

	/**
	 * ★PlayerController 가 창을 연 직후 딱 한 번 부른다★ (OpenSlidePuzzleScreen)
	 *
	 * 액터의 델리게이트를 구독하고, 지금까지의 공유 진행도를 그대로 그린다.
	 * 이게 빠지면 창은 뜨지만 빈 판이고 클릭해도 아무 일도 일어나지 않는다
	 * (위젯이 어느 액터를 보는지 몰라 보드도 못 받고 이동 RPC 도 못 보낸다).
	 */
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	void BindToPuzzle(ADRS2SlidePuzzle* InPuzzle);

	/** 마지막 이동 되돌리기 요청. ★히스토리는 팀 공용이라 남이 둔 수도 되돌아간다★ */
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	void RequestUndo();

	/** 그 판의 시작 배치로 복귀 요청. ★팀 전체의 진행도가 날아간다★ */
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	void RequestReset();

	/** 조작 점유권을 반납하고 창을 닫는다 (실제 제거·입력 모드 복귀는 PlayerController 가 한다). */
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	void RequestClose();

	/**
	 * 금고 번호 한 자리를 표시한다. 평소에는 복제된 RevealedDigit 를 받아 C++ 이 부른다.
	 * BP 는 연출 타이밍을 늦추고 싶을 때만 직접 부르면 된다. 음수를 주면 표시를 지운다.
	 */
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	void ShowCode(int32 Digit);

	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	bool IsSolved() const;

	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	bool IsInputLocked() const { return bInputLocked; }

	/** 서버가 센 이동 횟수 (팀 공용). */
	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	int32 GetMoveCount() const;

	/** 지금 조작 중인 사람 이름. 비어 있으면 아무도 안 만지고 있다. */
	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	FText GetOccupantName() const;

	/** 내가 지금 이 판을 움직일 수 있는가 (점유자이고, 아직 안 풀렸는가). */
	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	bool HasControl() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// ===== BindWidget =====

	/** 조각 8개가 붙을 캔버스. 디자이너에서는 비어 있다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CanvasPanel_Tiles;

	/**
	 * ★금고 번호가 뜨는 유일한 자리다★ (2026-09-20 - 월드 표시판을 쓰지 않는다)
	 * Optional 이라 없어도 컴파일은 되지만, 빠뜨리면 퍼즐을 풀어도 아무도 번호를 모른다.
	 */
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

	/** 한 칸 이동 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle", meta = (ClampMin = "0.02", ClampMax = "1.0"))
	float MoveDuration = 0.16f;

	/** Reset 시 8개가 동시에 움직이는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle", meta = (ClampMin = "0.02", ClampMax = "2.0"))
	float ResetDuration = 0.22f;

	/**
	 * 이동을 보냈는데 확정이 이 시간 안에 안 오면 잠금을 풀고 서버 보드로 다시 맞춘다.
	 *
	 * ★거절은 아무 답도 돌아오지 않는다★ - 보드가 안 바뀌면 복제도 없기 때문이다.
	 * 이 타이머가 없으면 서버가 한 번 거절할 때 입력이 영영 잠긴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float MoveAckTimeout = 0.5f;

	/** ESC 닫기 / Ctrl+Z 되돌리기 / R 초기화 단축키 사용 여부. */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle")
	bool bHandleKeyboardShortcuts = true;

	// ===== BP 연출 훅 =====

	/** 조각 8개 생성이 끝났다. BP 가 등장 연출을 여기서 시작한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnBoardBuilt();

	/** 빈 판이 아니라 진행 중인 공유 판을 열었다. 등장 연출을 줄이고 싶을 때 쓴다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnBoardResumed(int32 MoveCount);

	/** 서버가 확정한 이동이다 (보간은 지금부터). 사운드/카운터 갱신용. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnMoveApplied(int32 TileId, int32 FromIndex, int32 ToIndex);

	/** 갈 수 없는 조각을 눌렀다 (로컬 판정). 조각은 이미 흔들리고 있다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnInvalidMove(int32 TileId);

	/** 퍼즐이 풀렸다 (Anim_Solved 재생 직후). 숫자 공개는 OnCodeRevealed 로 따로 들어온다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnSolvedVisual();

	/** 금고 번호가 도착해 Text_Code 에 찍혔다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnCodeRevealed(int32 Digit);

	/** 입력 잠금 상태가 바뀌었다. 커서/버튼 외의 추가 연출용. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnInputLockChanged(bool bLocked);

	/** 조작 점유자가 바뀌었다. "OOO 조작 중" 표시용. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnOccupantChanged(APlayerState* Occupant);

	UFUNCTION()
	void HandleUndoClicked();

	UFUNCTION()
	void HandleResetClicked();

private:
	// ===== 서버 -> 화면 =====

	UFUNCTION()
	void HandleBoardChanged(const TArray<int32>& NewBoard, int32 NewMoveCount);

	UFUNCTION()
	void HandleDigitRevealed(int32 Digit);

	UFUNCTION()
	void HandleOccupantChanged(APlayerState* NewOccupant);

	/** 조각이 준비돼 있으면 액터의 현재 상태(보드 + 숫자)를 그대로 그린다. */
	void SyncFromPuzzle();

	/**
	 * 서버 보드를 화면에 반영한다. 이동·Undo·Reset·첫 오픈이 전부 여기로 들어오고,
	 * 무엇을 할지는 "내 화면과 몇 칸이 다른가"로 정해진다.
	 *   0칸  -> 이미 맞다 (할 일 없음)
	 *   2칸  -> 그 조각만 MoveDuration 으로 미끄러뜨린다
	 *   그 외 -> 전부 재배치 (첫 오픈은 즉시, 그 외는 ResetDuration)
	 */
	void ApplyServerBoard(const TArray<int32>& NewBoard);

	// ===== 화면 -> 서버 =====

	/** 이동/Undo/Reset 요청을 보낸 뒤 확정이 올 때까지 잠근다. */
	void BeginAckWait();
	void ClearAckWait();

	UFUNCTION()
	void HandleAckTimeout();

	/** 점유권 반납 (RequestClose 와 NativeDestruct 양쪽에서 부른다. 두 번 와도 안전하다). */
	void ReleaseControlOnServer();

	/**
	 * 창을 내리는 일을 PlayerController 에 맡긴다 (위젯 제거 · 입력 모드 복귀 둘 다 저쪽 책임).
	 * PC 를 못 찾으면 false 를 돌려주고, 그때만 위젯이 스스로 빠진다.
	 */
	bool CloseScreenOnController();

	// ===== 내부 =====

	/** 조각 위젯 8개 생성 + Canvas Slot 세팅. NativeConstruct 에서 1회. */
	void BuildTiles();

	void HandleTileClicked(int32 InTileId);
	void HandleSlideFinished(UDRS2PuzzleTileWidget* Tile);
	void HandleSolved();

	void SetInputLocked(bool bLocked);

	/** 보간·확정 대기·점유 상태를 종합해 잠금과 버튼을 한 번에 맞춘다. */
	void UpdateInteractivity();
	void RefreshButtons();

	// ===== 상태 =====

	/** 판의 주인. 이 참조가 없으면 위젯은 아무것도 못 한다. */
	TWeakObjectPtr<ADRS2SlidePuzzle> OwnerPuzzle;

	/**
	 * ★화면 사본이지 판정 근거가 아니다★
	 * 조각이 어느 칸에 있는지 그리고, 못 가는 조각을 즉시 흔들어 주는 데만 쓴다.
	 * 서버 보드가 내려오면 무조건 서버 쪽이 이긴다.
	 */
	TArray<int32> DisplayBoard;

	int32 EmptyIndex = DRS2Puzzle::CellCount - 1;

	UPROPERTY()
	TMap<int32, TObjectPtr<UDRS2PuzzleTileWidget>> TileWidgets;

	bool bInputLocked    = false;
	bool bSolvedOnce     = false;
	bool bBoardDrawnOnce = false;   // 첫 그리기는 애니메이션 없이 찍는다
	bool bWaitingForAck  = false;

	/** 아직 도착하지 않은 조각 수. 0 이 될 때만 잠금을 푼다 (Reset 은 8개가 동시에 끝난다). */
	int32 PendingSlides = 0;

	FTimerHandle AckTimer;
};
