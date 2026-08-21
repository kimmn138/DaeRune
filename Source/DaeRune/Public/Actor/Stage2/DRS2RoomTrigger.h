// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2RoomTrigger.generated.h"

class UBoxComponent;
class ADRCharacter;

// 살아있는 플레이어 전원이 방 안에 들어왔을 때 1회 발화
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllAlivePlayersInside);
// 입장 인원 변화 (목표 UI "n/N" 표시용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInsideCountChanged, int32, InsideCount, int32, AliveTotal);

/**
 * 스테이지2 방 입장 판정 트리거 (Plan6 §4.2)
 *
 * 서버 전용 로직. 박스 오버랩으로 방 안의 플레이어를 추적하고
 * GameState의 생존 플레이어 목록과 대조해 "전원 입장"을 판정한다.
 *
 * 오버랩만으로는 부족하다: 방 밖에서 누군가 죽으면 오버랩 이벤트가 발생하지 않아
 * 조건이 영원히 성립하지 않는다. 그래서 Arm 상태 동안 주기적으로 재평가한다.
 */
UCLASS()
class DAERUNE_API ADRS2RoomTrigger : public AActor
{
	GENERATED_BODY()

public:
	ADRS2RoomTrigger();

	// 감시 시작 (서버). 페이즈가 자기 수명에 맞춰 호출한다.
	UFUNCTION(BlueprintCallable, Category = "S2|Trigger")
	void Arm();

	// 감시 중지 (서버)
	UFUNCTION(BlueprintCallable, Category = "S2|Trigger")
	void Disarm();

	// 발화 래치만 해제해 다시 판정 가능하게 만든다.
	// 방3의 부품 동반 검증처럼 "조건 미달로 발화를 무시한" 경우에 사용한다.
	UFUNCTION(BlueprintCallable, Category = "S2|Trigger")
	void ReArm();

	// 즉시 질의 (완료 판정 등에 사용)
	UFUNCTION(BlueprintCallable, Category = "S2|Trigger")
	bool AreAllAlivePlayersInside() const;

	// 특정 액터가 트리거 박스 안에 있는지 (부품 동반 검증용)
	UFUNCTION(BlueprintCallable, Category = "S2|Trigger")
	bool IsActorInside(const AActor* TargetActor) const;

	FName GetRoomID() const { return RoomID; }

	// 서버 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "S2|Trigger")
	FOnAllAlivePlayersInside OnAllInside;

	UPROPERTY(BlueprintAssignable, Category = "S2|Trigger")
	FOnInsideCountChanged OnCountChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Trigger")
	TObjectPtr<UBoxComponent> TriggerBox;

	// 배선 검증/로그용 식별자 (예: "Room1")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Trigger")
	FName RoomID = NAME_None;

	// Arm 상태에서 재평가 주기 (사망/리스폰 등 오버랩 이벤트가 없는 변화 대응)
	UPROPERTY(EditDefaultsOnly, Category = "S2|Trigger", meta = (ClampMin = "0.1"))
	float ReevaluateInterval = 0.5f;

private:
	// 조건 평가 후 필요 시 델리게이트 발화 (서버 전용)
	void Evaluate();

	// 오버랩 집합에서 유효하지 않은 항목 제거
	void PruneInvalidPlayers();

	UPROPERTY()
	TSet<TWeakObjectPtr<ADRCharacter>> PlayersInside;

	FTimerHandle ReevaluateTimer;

	bool bArmed = false;
	bool bFired = false;

	// 같은 값을 반복 브로드캐스트하지 않기 위한 캐시
	int32 LastBroadcastInside = -1;
	int32 LastBroadcastTotal = -1;
};
