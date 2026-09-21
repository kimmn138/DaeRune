// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Phase/Stage2/DRS2Types.h"
#include "DRS2SlidePuzzle.generated.h"

class ADRCharacter;
class APlayerController;
class APlayerState;

// 보드가 바뀌었다 (이동 / Undo / Reset / 최초 셔플). 열려 있는 위젯이 이걸 받아 화면을 맞춘다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDRS2OnSlideBoardChanged, const TArray<int32>&, NewBoard, int32, MoveCount);
// 금고 번호 한 자리가 공개됐다 (해결 후에만 발화한다).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRS2OnSlideDigitRevealed, int32, Digit);
// 조작 점유자가 바뀌었다. null 이면 아무도 만지고 있지 않다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDRS2OnSlideOccupantChanged, APlayerState*, Occupant);

/**
 * 8퍼즐 (Plan6 §14.2.2) - ★UI 방식 + 서버 권위 (2026-09-20 개정)★
 *
 * 월드 단말(ADRS2PuzzleTerminal)로 상호작용하면 조작한 플레이어 화면에만 UI 가 뜨고,
 * 퍼즐을 풀면 그 UI 안에 금고 비밀번호 한 자리가 표시된다. 월드 표시판(ADRS2CodeScreen)은 쓰지 않는다.
 *
 * ★판의 주인은 이 액터다★
 *   요구사항이 "조작은 동시에 1명, 진행도는 팀 전체 공유" 이므로, 보드가 위젯에 있으면 성립하지 않는다
 *   (위젯은 연 사람의 클라에만 존재하고, 창을 닫으면 판도 사라진다).
 *   그래서 Board / History / 점유권을 여기서 들고 Board 를 복제한다. 위젯은 그것을 그리는 뷰다.
 *
 * 책임:
 *   1) 판 생성 (BeginPlay 에서 1회 셔플) 과 복제
 *   2) 조작 점유권 - 동시에 한 명만 조작할 수 있다
 *   3) 이동 검증 (인접 판정) · Undo · Reset - 클라의 요청을 그대로 믿지 않는다
 *   4) 정답 판정과 숫자 공개
 *
 * 확정된 계약:
 *   - 정답 숫자는 페이즈가 SetRevealDigit 으로 주입한다 (§14.2.6).
 *   - SecretDigit 은 서버 전용이고, 해결 전에는 RevealedDigit 이 -1 로 복제되어
 *     클라에 정답 정보가 아예 존재하지 않는다.
 *   - 조작 요청은 반드시 ADRPlayerController 의 Server RPC 를 거친다
 *     (이 액터는 클라가 소유하지 않아 Server RPC 를 직접 받을 수 없다).
 */
UCLASS()
class DAERUNE_API ADRS2SlidePuzzle : public AActor
{
	GENERATED_BODY()

public:
	ADRS2SlidePuzzle();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 페이즈가 매 판 생성한 비밀번호 한 자리를 주입한다 (서버 전용, 복제되지 않는다).
	void SetRevealDigit(int32 InDigit);

	// ===== 단말에서 들어오는 요청 (서버) =====

	/**
	 * 단말 상호작용 -> 점유 시도 후 해당 플레이어에게 UI 열기 (서버).
	 *
	 * - 이미 해결된 판이면 점유 없이 누구나 연다 (번호 확인용 읽기 전용).
	 * - 다른 사람이 조작 중이면 열지 않고 Client_SlidePuzzleBusy 로 알린다.
	 */
	void RequestOpenUI(ADRCharacter* Character);

	/** 창을 닫았다 / 위젯이 사라졌다 -> 점유 반납 (서버). 점유자가 아니면 무시한다. */
	void ReleaseControl(APlayerController* PlayerController);

	// ===== 조작 (서버 · PlayerController 의 Server RPC 가 호출한다) =====

	/** 조각 한 칸 이동. 점유자 + 인접 조건을 모두 통과해야 보드가 바뀐다. */
	bool TryMove(APlayerController* PlayerController, int32 TileId);

	/** 마지막 이동 되돌리기. 히스토리는 팀 공용이다 (A 가 둔 수를 B 가 되돌릴 수 있다). */
	bool TryUndo(APlayerController* PlayerController);

	/** 그 판의 시작 배치로 복귀. ★팀 전체의 진행도가 날아간다★ */
	bool TryReset(APlayerController* PlayerController);

	// ===== 읽기 (클라 포함) =====

	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	const TArray<int32>& GetBoard() const { return Board; }

	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	int32 GetMoveCount() const { return MoveCount; }

	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	bool IsSolved() const { return bSolved; }

	// 공개된 숫자 (해결 전에는 -1)
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	int32 GetRevealedDigit() const { return RevealedDigit; }

	/** 지금 조작 중인 사람. null 이면 비어 있다. */
	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	APlayerState* GetOccupant() const { return Occupant; }

	/** 이 컨트롤러가 아닌 다른 누군가가 조작 중인가 (단말 프롬프트 표시용). */
	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	bool IsOccupiedByOther(const APlayerController* PlayerController) const;

	// ===== 위젯이 구독하는 창구 =====

	UPROPERTY(BlueprintAssignable, Category = "S2|Puzzle")
	FDRS2OnSlideBoardChanged OnBoardChanged;

	UPROPERTY(BlueprintAssignable, Category = "S2|Puzzle")
	FDRS2OnSlideDigitRevealed OnDigitRevealed;

	UPROPERTY(BlueprintAssignable, Category = "S2|Puzzle")
	FDRS2OnSlideOccupantChanged OnOccupantChanged;

	// 서버 델리게이트 (페이즈가 진행도 표시에 사용)
	UPROPERTY(BlueprintAssignable, Category = "S2|Puzzle")
	FOnS2PuzzleSolved OnPuzzleSolved;

	/**
	 * 테스트 전용. 보드를 정답 배치로 밀어넣고 해결 처리까지 태운다.
	 * ★서버에서만 의미가 있다★ (클라에서 부르면 조용히 아무 일도 없다)
	 */
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle|Debug", meta = (DevelopmentOnly))
	void DebugSolveInstantly();

protected:
	virtual void BeginPlay() override;

	/**
	 * 해결 처리 (서버 내부 전용).
	 *
	 * ★BlueprintCallable 이 아니다★ - 클라가 부를 수 있는 "해결 보고" 경로를 두면
	 * 그게 곧 퍼즐을 건너뛰는 치트가 된다. 판정은 서버가 TryMove 안에서 직접 한다.
	 */
	void NotifySolved();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Puzzle")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 정답에서 출발해 합법 이동을 몇 번 무작위로 밟을지. 20 미만이면 눈에 띄게 쉬워진다. */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle", meta = (ClampMin = "20", ClampMax = "500"))
	int32 ShuffleSteps = 60;

	// ===== 복제되는 판 =====

	/** 길이 9. 값 = TileId(1~8), 0 = 빈칸. 인덱스 = Row * GridSize + Col */
	UPROPERTY(ReplicatedUsing = OnRep_Board, BlueprintReadOnly, Category = "S2|Puzzle")
	TArray<int32> Board;

	/**
	 * 지금까지의 이동 횟수.
	 * Board 와 한 묶음으로 복제되므로 OnRep_Board 안에서 읽어도 옛 값이 나오지 않는다
	 * (같은 액터의 프로퍼티는 전부 적용된 뒤에 RepNotify 가 불린다).
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "S2|Puzzle")
	int32 MoveCount = 0;

	/** 지금 조작 중인 플레이어. 동시 조작 1명 규칙의 전부다. */
	UPROPERTY(ReplicatedUsing = OnRep_Occupant, BlueprintReadOnly, Category = "S2|Puzzle")
	TObjectPtr<APlayerState> Occupant;

	UPROPERTY(ReplicatedUsing = OnRep_RevealedDigit, BlueprintReadOnly, Category = "S2|Puzzle")
	int32 RevealedDigit = -1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "S2|Puzzle")
	bool bSolved = false;

	// ===== 서버 전용 =====

	// 서버 전용 정답 숫자. 해결 전까지 클라로 내려보내지 않는다.
	int32 SecretDigit = -1;

	/** 이번 판의 시작 배치. Reset 이 이걸 되돌린다. */
	TArray<int32> StartBoard;

	/** 움직인 TileId 순서. 같은 조각을 한 번 더 움직이면 제자리로 오므로 좌표는 필요 없다. */
	TArray<int32> History;

	UFUNCTION()
	void OnRep_Board();

	UFUNCTION()
	void OnRep_Occupant();

	UFUNCTION()
	void OnRep_RevealedDigit();

	// 해결 연출 (전 클라). 위젯은 OnDigitRevealed 로 받으므로 이건 액터 쪽 추가 연출용이다.
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnPuzzleSolvedVisual(int32 Digit);

private:
	/** 점유자 교체 + 알림. 서버에서는 RepNotify 가 저절로 불리지 않으므로 여기서 직접 돌린다. */
	void SetOccupant(APlayerState* NewOccupant);

	/** 지금 이 컨트롤러가 조작권을 갖고 있는가 (서버 판정). */
	bool HasControl(const APlayerController* PlayerController) const;

	/** 인접 검사 후 보드를 스왑한다. 히스토리/카운터는 건드리지 않는다. */
	bool ApplyMoveInternal(int32 TileId);
};
