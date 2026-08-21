// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Phase/Stage2/DRS2Types.h"
#include "DRS2SlidePuzzle.generated.h"

class ADRS2CodeScreen;
class ADRCharacter;

/**
 * 8퍼즐 (Plan6 §14.2.2) - ★UI 방식으로 변경됨 (2026-08-07)
 *
 * 당초 설계는 월드에 배치된 3x3 타일 모델을 직접 미는 방식이었으나,
 * UI 창을 띄워 조작하는 방식으로 변경되었다.
 * 따라서 이 액터는 아래 두 가지만 담당하고, **퍼즐 내부 로직은 아직 비어 있다**:
 *
 *   1) 월드 단말(ADRS2PuzzleTerminal)로 상호작용하면 조작한 플레이어에게 UI를 띄운다.
 *   2) 퍼즐이 풀리면 금고 비밀번호 첫 자리를 공개한다.
 *
 * 미구현 (퍼즐 세부 사양 확정 후 작성):
 *   - 보드 상태 복제, 타일 이동 검증, 되돌리기/초기화, 해결 판정
 *   - UI 위젯과 서버 사이의 조작 RPC
 *
 * 확정된 계약:
 *   - 정답 숫자는 페이즈가 SetRevealDigit 으로 주입한다 (§14.2.6).
 *   - 해결 전에는 RevealedDigit 이 -1 로 복제되어 클라에 정답 정보가 존재하지 않는다.
 *   - 퍼즐 로직이 서버에서 NotifySolved() 를 호출하면 공개 + 델리게이트 발화가 처리된다.
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

	// 단말 상호작용 -> 해당 플레이어에게 UI 열기 요청 (서버).
	// 이미 해결된 퍼즐이면 열지 않는다.
	void RequestOpenUI(ADRCharacter* Character);

	// ★퍼즐 로직 진입점: UI(또는 추후 구현될 서버 로직)가 정답에 도달했을 때 서버에서 호출한다.
	// 1회만 처리되며 숫자 공개 + OnPuzzleSolved 발화를 담당한다.
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	void NotifySolved();

	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	bool IsSolved() const { return bSolved; }

	// 공개된 숫자 (해결 전에는 -1)
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	int32 GetRevealedDigit() const { return RevealedDigit; }

	// 서버 델리게이트 (페이즈가 진행도 표시에 사용)
	UPROPERTY(BlueprintAssignable, Category = "S2|Puzzle")
	FOnS2PuzzleSolved OnPuzzleSolved;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Puzzle")
	TObjectPtr<USceneComponent> SceneRoot;

	// 해결 시 숫자를 표시할 스크린
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Puzzle")
	TObjectPtr<ADRS2CodeScreen> CodeScreen;

	// 서버 전용 정답 숫자. 해결 전까지 클라로 내려보내지 않는다.
	int32 SecretDigit = -1;

	UPROPERTY(ReplicatedUsing = OnRep_RevealedDigit, BlueprintReadOnly, Category = "S2|Puzzle")
	int32 RevealedDigit = -1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "S2|Puzzle")
	bool bSolved = false;

	UFUNCTION()
	void OnRep_RevealedDigit();

	// 해결 연출 (전 클라)
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnPuzzleSolvedVisual(int32 Digit);
};
