// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Phase/Stage2/DRS2PhaseBase.h"
#include "DRS2PuzzlePhase.generated.h"

class ADRCharacter;
class ADRCleanserPart;

/**
 * 스테이지2 페이즈 1 - 방2 퍼즐 3종 + 금고 (Plan6 §14.2)
 *
 * 흐름:
 *   D1 게이트 발광 -> 순간이동으로 방2 진입
 *   -> 8퍼즐(UI) / 스위치 / CCTV 로 금고 3자리 확보 -> 금고 개방 -> 부품 획득
 *   -> 부품 소지자가 출구 통과 -> 생존자 전원 방1 회수 + D1 비활성 -> 완료
 *
 * 완료 조건이 "부품 픽업"이 아니라 "부품 소지자의 퇴장"인 점에 주의한다.
 * 픽업 후 드롭해도 목표는 유지되고, 누구든 다시 들고 나가면 완료된다.
 *
 * 비밀번호 생성/배분은 이 페이즈가 담당한다 (§14.2.6).
 * 각 액터가 독립적으로 난수를 뽑으면 값이 어긋나므로 반드시 한곳에서 만들어 주입한다.
 */
UCLASS(Blueprintable)
class DAERUNE_API UDRS2PuzzlePhase : public UDRS2PhaseBase
{
	GENERATED_BODY()

public:
	virtual void OnPhaseStart() override;
	virtual void OnPhaseEnd() override;
	virtual bool IsCompleted() const override;

protected:
	// ========== 콜백 ==========

	// 8퍼즐/스위치 퍼즐이 숫자를 공개했을 때
	UFUNCTION()
	void HandlePuzzleSolved(AActor* Puzzle);

	// 금고 개방 (내부 부품 스폰됨)
	UFUNCTION()
	void HandleSafeOpened(AActor* SpawnedPart);

	// 금고에서 나온 부품을 누군가 획득
	UFUNCTION()
	void HandlePartPickedUp(ADRCleanserPart* Part, ADRCharacter* Character);

	// 부품 소지자가 방2 출구를 통과해 전원이 회수됨
	UFUNCTION()
	void HandleTeamRecalled();

	// ========== 상태 ==========

	// 서버 전용 3자리 코드
	TArray<uint8> SecretCode;

	// 스크린에 숫자가 뜬 퍼즐 수 (0~2). CCTV는 해결 개념이 없어 포함하지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "S2|Room2")
	int32 DigitsRevealed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "S2|Room2")
	bool bSafeOpened = false;

	UPROPERTY(BlueprintReadOnly, Category = "S2|Room2")
	bool bPartPickedUp = false;

	UPROPERTY(BlueprintReadOnly, Category = "S2|Room2")
	bool bCarrierExited = false;
};
