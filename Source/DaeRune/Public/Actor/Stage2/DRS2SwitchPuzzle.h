// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Phase/Stage2/DRS2Types.h"
#include "DRS2SwitchPuzzle.generated.h"

class ADRS2CodeScreen;

/**
 * 스위치 퍼즐 (Plan6 §14.2.3)
 *
 * 레버 5개, 전구 9개. 레버마다 켜고 끌 수 있는 전구 조합이 다르다.
 * 모든 전구를 켜는 조합을 찾으면 1라운드 성공이며, 3라운드를 성공해야 클리어다.
 * 성공할 때마다 성공 표시가 1개씩 켜지고 레버-전구 조합이 새로 생성된다.
 *
 * 전구 계산은 XOR(라이트아웃 방식)이다. OR 누적이면 레버를 전부 켜기만 하면 끝나
 * 퍼즐이 성립하지 않는다.
 *
 * ★해가 존재하는 마스크 생성이 필수다. 마스크를 완전 랜덤으로 뽑으면 32개 조합 중
 *   정답이 없는 라운드가 나와 소프트락이 된다. 정답 부분집합에서 역산한 뒤 완전탐색으로 검증한다.
 *
 * 마스크는 복제하지 않는다. 클라에는 전구/레버 표시 상태와 성공 횟수만 내려간다.
 */
UCLASS()
class DAERUNE_API ADRS2SwitchPuzzle : public AActor
{
	GENERATED_BODY()

public:
	ADRS2SwitchPuzzle();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 페이즈가 매 판 생성한 비밀번호 한 자리를 주입한다 (서버 전용)
	void SetRevealDigit(int32 InDigit);

	// 레버 토글 (서버). ADRS2Lever 가 호출한다.
	void ToggleLever(int32 LeverIndex);

	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	bool IsSolved() const { return bSolved; }

	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	int32 GetRevealedDigit() const { return RevealedDigit; }

	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	int32 GetRoundsCleared() const { return RoundsCleared; }

	UPROPERTY(BlueprintAssignable, Category = "S2|Puzzle")
	FOnS2PuzzleSolved OnPuzzleSolved;

protected:
	virtual void BeginPlay() override;

	// ========== 설정 ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Puzzle", meta = (ClampMin = "1"))
	int32 LeverCount = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Puzzle", meta = (ClampMin = "1", ClampMax = "16"))
	int32 BulbCount = 9;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Puzzle", meta = (ClampMin = "1"))
	int32 RequiredRounds = 3;

	// 레버 1개가 담당하는 전구 수 범위 (난이도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Puzzle", meta = (ClampMin = "1"))
	int32 MinBitsPerLever = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Puzzle", meta = (ClampMin = "1"))
	int32 MaxBitsPerLever = 5;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Puzzle")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Puzzle")
	TObjectPtr<ADRS2CodeScreen> CodeScreen;

	// ========== 복제 상태 ==========

	// 전구 점등 상태 비트마스크
	UPROPERTY(ReplicatedUsing = OnRep_BulbBits, BlueprintReadOnly, Category = "S2|Puzzle")
	int32 BulbBits = 0;

	// 레버 ON/OFF 시각 상태 비트마스크
	UPROPERTY(ReplicatedUsing = OnRep_LeverBits, BlueprintReadOnly, Category = "S2|Puzzle")
	int32 LeverBits = 0;

	UPROPERTY(ReplicatedUsing = OnRep_RoundsCleared, BlueprintReadOnly, Category = "S2|Puzzle")
	int32 RoundsCleared = 0;

	UPROPERTY(ReplicatedUsing = OnRep_RevealedDigit, BlueprintReadOnly, Category = "S2|Puzzle")
	int32 RevealedDigit = -1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "S2|Puzzle")
	bool bSolved = false;

	UFUNCTION() void OnRep_BulbBits();
	UFUNCTION() void OnRep_LeverBits();
	UFUNCTION() void OnRep_RoundsCleared();
	UFUNCTION() void OnRep_RevealedDigit();

	// ========== 연출 훅 ==========

	// 전구 점등 상태 갱신 (비트 i = 전구 i)
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnBulbsChanged(int32 NewBulbBits);

	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnLeversChanged(int32 NewLeverBits);

	// 성공 표시 갱신 (n/3)
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnRoundsClearedChanged(int32 NewRoundsCleared);

	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnPuzzleSolvedVisual(int32 Digit);

private:
	// 해가 존재하는 마스크 세트를 생성한다 (서버)
	void GenerateRound();

	// 현재 켜진 레버들의 마스크 XOR
	int32 ComputeBulbBits() const;

	// 라운드 성공 판정
	void CheckRound();

	// 32(2^LeverCount) 조합 완전탐색으로 정답 존재 여부 확인
	bool HasSolution() const;

	// 무작위 마스크 1개 (MinBitsPerLever ~ MaxBitsPerLever 개의 비트)
	int32 MakeRandomMask() const;

	// 전구 전체 점등 비트마스크
	int32 GetAllBulbsOnMask() const { return (1 << BulbCount) - 1; }

	// 서버 전용: 레버별 전구 마스크 (복제하지 않는다 - 정답 정보)
	TArray<int32> LeverMasks;

	// 서버 전용 정답 숫자
	int32 SecretDigit = -1;
};
