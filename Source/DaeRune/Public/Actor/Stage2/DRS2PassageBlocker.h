// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2PassageBlocker.generated.h"

class UBoxComponent;

/**
 * 스테이지2 통로 차단 액터 공용 베이스 (Plan6 §4.11)
 *
 * 통로를 막고/여는 액터의 공통 계약을 정의한다. 파생 클래스가 "어떻게 움직이는가"만 구현하고,
 * 상태 복제·콜리전 토글·끼임 방어는 이 베이스가 처리한다.
 *
 *   ADRS2MovingBlocker  : 구조물이 위아래로 이동
 *   ADRS2TurnstileDoor  : 지하철 개찰구식 회전 문
 *
 * 페이즈는 이 베이스 타입만 알고 SetBlocked(bool) 로 제어하므로,
 * 통로마다 어떤 방식을 쓰든 페이즈 코드는 바뀌지 않는다.
 *
 * 애니메이션은 "1회 복제 + 로컬 시뮬" 모델이다. bBlocked 하나만 복제하고
 * 서버/클라가 각자 동일한 보간을 수행한다 (이동 자체는 복제하지 않는다).
 */
UCLASS(Abstract)
class DAERUNE_API ADRS2PassageBlocker : public AActor
{
	GENERATED_BODY()

public:
	ADRS2PassageBlocker();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 전용. 같은 값으로 호출하면 아무것도 하지 않는다(멱등).
	// D4는 방3 페이즈와 방5 페이즈가 모두 개방을 호출할 수 있으므로 멱등성이 필요하다.
	UFUNCTION(BlueprintCallable, Category = "S2|Blocker")
	void SetBlocked(bool bNewBlocked);

	UFUNCTION(BlueprintCallable, Category = "S2|Blocker")
	bool IsBlocked() const { return bBlocked; }

protected:
	virtual void BeginPlay() override;

	// ===== 파생 클래스가 구현할 것 =====

	// 애니메이션 진행도를 포즈에 반영한다.
	// Alpha 0 = 열림 포즈, 1 = 막힘 포즈
	virtual void ApplyPose(float Alpha) PURE_VIRTUAL(ADRS2PassageBlocker::ApplyPose, );

	// 초기 포즈를 잡기 전에 파생 클래스가 필요한 것을 준비한다 (컴포넌트 수집, 원점 회전 캐시 등)
	virtual void InitializePose() {}

	// ===== 공통 구성 =====

	// 통로 차단 콜리전 (Pawn만 Block). 움직이지 않고 콜리전만 토글된다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Blocker")
	TObjectPtr<UBoxComponent> PawnBlock;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Blocker", meta = (ClampMin = "0.05"))
	float MoveDuration = 1.5f;

	// 레벨 배치 시 초기 상태. D0/D2 = false(열림 시작), D4/D5 = true(막힘 시작)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Blocker")
	bool bStartBlocked = false;

	// 막힘 완료 시 통로 안에 남아 있는 Pawn을 옮길 지점 (끼임 방어).
	// 미지정이면 밀어내기를 수행하지 않는다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Blocker")
	TObjectPtr<AActor> PushOutPoint;

	UPROPERTY(ReplicatedUsing = OnRep_bBlocked, BlueprintReadOnly, Category = "S2|Blocker")
	bool bBlocked = false;

	UFUNCTION()
	void OnRep_bBlocked();

	// 연출 훅 (사운드/카메라 셰이크/VFX)
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Blocker")
	void OnMoveStarted(bool bNowBlocking);

	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Blocker")
	void OnMoveFinished(bool bNowBlocking);

private:
	void BeginMove();
	void FinishMove();
	void PushOutTrappedPawns();

	// 현재 애니메이션 진행도 (0 = 열림, 1 = 막힘)
	float CurrentAlpha = 0.f;

	float MoveFromAlpha = 0.f;
	float MoveToAlpha = 0.f;
	float MoveElapsed = 0.f;
	bool bMoving = false;
};
