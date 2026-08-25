// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Phase/Stage2/DRS2Types.h"
#include "DRS2SwitchPuzzle.generated.h"

class ADRS2CodeScreen;
class ADRS2Lever;

/**
 * 레버 1개가 담당하는 전구 목록 (Plan6 §14.2.3)
 *
 * 비트마스크 대신 전구 번호 배열로 받는다. 에디터에서 0b000101011 을 계산해 넣는 것보다
 * "0, 1, 3" 을 적는 편이 훨씬 다루기 쉽기 때문이다. 내부에서 마스크로 변환한다.
 */
USTRUCT(BlueprintType)
struct FS2LeverBulbs
{
	GENERATED_BODY()

	// 이 레버가 뒤집는 전구 번호들 (0 ~ BulbCount-1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S2|Switch")
	TArray<int32> BulbIndices;
};

/**
 * 스위치 퍼즐 조합 프리셋 1개 (Plan6 §14.2.3)
 *
 * 레버 전체의 담당 전구를 한 벌로 묶은 것. 매 라운드 이 중 하나를 랜덤으로 고른다.
 * 배열 순서 = 레버 번호(PropIndex)다.
 */
USTRUCT(BlueprintType)
struct FS2SwitchPreset
{
	GENERATED_BODY()

	// 레버 순서대로. 개수가 LeverCount 와 같아야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S2|Switch")
	TArray<FS2LeverBulbs> Levers;
};

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

	// 레버가 BeginPlay 에서 자기를 등록한다 (서버·클라 공통).
	// 등록 즉시 현재 LeverBits 기준 자세를 밀어주므로 BeginPlay 순서에 의존하지 않는다.
	void RegisterLever(ADRS2Lever* Lever);

	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	bool IsSolved() const { return bSolved; }

	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	int32 GetRevealedDigit() const { return RevealedDigit; }

	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	int32 GetRoundsCleared() const { return RoundsCleared; }

	// BP 편의: 비트마스크의 Index 번째 비트가 켜졌는지.
	// OnBulbsChanged / OnLeversChanged 에서 ForLoop 인덱스와 함께 쓴다.
	// (BP 에는 시프트 노드가 없어 비트 검사를 직접 짜기 번거롭다)
	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	static bool IsBitSet(int32 Bits, int32 Index);

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

	// ★조합 프리셋. 매 라운드 이 중 하나를 비반복 랜덤으로 고른다.
	//   BeginPlay 에서 "해가 존재하는지"를 전부 검증하고, 통과한 것만 사용한다.
	//   비워두면 아래 절차적 생성으로 폴백한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Switch|프리셋")
	TArray<FS2SwitchPreset> Presets;

	// 사용 가능한 프리셋이 하나도 없을 때 랜덤 생성으로 대체할지.
	// 끄면 프리셋이 없을 때 퍼즐이 시작되지 않는다(Error 로그).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Switch|프리셋")
	bool bAllowProceduralFallback = true;

	// ===== 절차적 생성 폴백 설정 (프리셋이 없을 때만 사용) =====

	// 레버 1개가 담당하는 전구 수 범위 (난이도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Switch|폴백", meta = (ClampMin = "1"))
	int32 MinBitsPerLever = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Switch|폴백", meta = (ClampMin = "1"))
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
	// 라운드 시작: 프리셋에서 뽑거나(우선) 절차적으로 생성한다 (서버)
	void GenerateRound();

	// 현재 켜진 레버들의 마스크 XOR
	int32 ComputeBulbBits() const;

	// 라운드 성공 판정
	void CheckRound();

	// 2^LeverCount 조합 완전탐색으로 정답 존재 여부 확인
	bool HasSolution(const TArray<int32>& Masks) const;

	// ===== 프리셋 =====

	// BeginPlay 1회: 프리셋을 마스크로 변환하고 검증해 ValidPresetMasks 를 채운다 (서버)
	void BuildValidPresets();

	// 전구 번호 배열 -> 비트마스크. 범위 밖 번호는 경고 후 무시한다.
	int32 MaskFromBulbIndices(const TArray<int32>& BulbIndices, int32 PresetIndex, int32 LeverIndex) const;

	// 비반복 랜덤으로 프리셋 1개 선택. 사용 가능한 것이 없으면 false.
	bool PickPresetMasks(TArray<int32>& OutMasks);

	// ===== 절차적 생성 (폴백) =====

	// 해가 존재하는 마스크 세트를 랜덤 생성한다
	void GenerateProceduralMasks(TArray<int32>& OutMasks) const;

	// 무작위 마스크 1개 (MinBitsPerLever ~ MaxBitsPerLever 개의 비트)
	int32 MakeRandomMask() const;

	// 검증을 통과한 프리셋들 (서버 전용, 마스크로 변환된 상태).
	// 중첩 TArray 는 리플렉션이 지원하지 않으므로 UPROPERTY 를 붙이지 않는다 (POD 라 GC 무관).
	TArray<TArray<int32>> ValidPresetMasks;

	// 아직 사용하지 않은 프리셋 인덱스 풀 (비면 전체로 다시 채운다).
	// 3라운드 동안 같은 조합이 반복되지 않게 한다 (스폰 지점 선택과 같은 관례).
	TArray<int32> RemainingPresetIndices;

	// 전구 전체 점등 비트마스크
	int32 GetAllBulbsOnMask() const { return (1 << BulbCount) - 1; }

	// 등록된 레버들에 현재 LeverBits 기준 자세를 밀어준다
	void RefreshLeverPoses();

	// 자기를 등록한 레버들 (자세 갱신 대상). 서버·클라 양쪽에 존재한다.
	UPROPERTY()
	TArray<TObjectPtr<ADRS2Lever>> RegisteredLevers;

	// 서버 전용: 레버별 전구 마스크 (복제하지 않는다 - 정답 정보)
	TArray<int32> LeverMasks;

	// 서버 전용 정답 숫자
	int32 SecretDigit = -1;
};
