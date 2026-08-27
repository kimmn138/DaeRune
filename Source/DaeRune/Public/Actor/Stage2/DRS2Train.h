// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2Train.generated.h"

class ADRS2TrainTrack;
class ADRS2TrainCar;
class ADRCharacter;

UENUM(BlueprintType)
enum class ES2TrainState : uint8
{
	WaitingForBoarding	UMETA(DisplayName = "Waiting For Boarding"),
	Moving				UMETA(DisplayName = "Moving"),
	StoppedAtObstacle	UMETA(DisplayName = "Stopped At Obstacle"),
	Arrived				UMETA(DisplayName = "Arrived")
};

/**
 * 열차 이동 상태 스냅샷 (Plan6 §4.9.2)
 *
 * "1회 복제 + 로컬 시뮬" 모델. 이 구조체 하나를 복제하고 서버/클라가 각자 동일 계산으로
 * 위치를 구한다. 이동 자체를 복제하지 않으므로(bReplicateMovement = false) 소스가 이중이 되지 않는다.
 */
USTRUCT(BlueprintType)
struct FS2TrainMovement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) ES2TrainState State = ES2TrainState::WaitingForBoarding;
	UPROPERTY(BlueprintReadOnly) float StartDistance = 0.f;
	UPROPERTY(BlueprintReadOnly) float TargetDistance = 0.f;
	UPROPERTY(BlueprintReadOnly) float StartServerTime = 0.f;
	UPROPERTY(BlueprintReadOnly) float Speed = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTrainBoardingChanged, int32, SeatedCount, int32, AliveTotal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrainStoppedAtTarget, int32, TargetIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTrainArrived);

/**
 * 열차 (Plan6 §4.9.2 / §14.6.2)
 *
 * 4칸 열차. 각 칸에 좌석 1개가 붙고 생존자 전원이 착석하면 등속으로 출발한다.
 * 좌석과 탑승자는 attach 계층이라 열차 이동을 자동으로 따라온다.
 */
UCLASS()
class DAERUNE_API ADRS2Train : public AActor
{
	GENERATED_BODY()

public:
	ADRS2Train();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ========== 서버 API (페이즈가 호출) ==========

	// 목표 거리까지 등속 이동 시작. TargetIndex 는 도착 시 알림에 실어 보낸다(장애물 인덱스).
	void DepartTo(float InTargetDistance, float InSpeed, int32 InTargetIndex);

	// 탑승 대기 상태로 전환 (출발 전 / 장애물 정지 후 재탑승 국면)
	void SetWaitingForBoarding();

	// 종점 도착 상태로 전환
	void SetArrived();

	// 좌석 점유 변화 통지 (좌석이 호출)
	void NotifySeatOccupancyChanged();

	// ========== 질의 ==========

	// 탑승을 받는 상태인지 (주행 중에는 못 탄다)
	UFUNCTION(BlueprintCallable, Category = "S2|Train")
	bool IsAcceptingBoarding() const;

	// 지금 하차해도 되는지 (정지 중에만 허용 - §14.6.5)
	UFUNCTION(BlueprintCallable, Category = "S2|Train")
	bool CanDeboardNow() const;

	// 생존자 전원이 착석했는지 (빈 칸 허용 - 조건은 "생존자 전원 착석")
	UFUNCTION(BlueprintCallable, Category = "S2|Train")
	bool AreAllAlivePlayersSeated() const;

	UFUNCTION(BlueprintCallable, Category = "S2|Train")
	ES2TrainState GetTrainState() const { return Movement.State; }

	UFUNCTION(BlueprintCallable, Category = "S2|Train")
	float GetCurrentDistance() const { return CurrentDistance; }

	// 전 칸 강제 하차 (종점 등)
	void DeboardAll();

	const TArray<TObjectPtr<ADRS2TrainCar>>& GetCars() const { return Cars; }

	// 선두 칸이 지나온 스플라인 거리
	UFUNCTION(BlueprintCallable, Category = "S2|Train")
	float GetHeadDistance() const { return CurrentDistance; }

	// 서버 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "S2|Train")
	FOnTrainBoardingChanged OnBoardingChanged;

	UPROPERTY(BlueprintAssignable, Category = "S2|Train")
	FOnTrainStoppedAtTarget OnStoppedAtTarget;

	UPROPERTY(BlueprintAssignable, Category = "S2|Train")
	FOnTrainArrived OnArrived;

protected:
	virtual void BeginPlay() override;

	// 레벨에서 배선하는 선로
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Train")
	TObjectPtr<ADRS2TrainTrack> Track;

	// ★열차 칸(바구니) 목록. 레벨에 배치한 ADRS2TrainCar 들을 **선두부터 순서대로** 배선한다.
	//   배열 순서가 곧 칸 번호이며, 칸 번호만큼 뒤처진 거리에 배치된다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Train")
	TArray<TObjectPtr<ADRS2TrainCar>> Cars;

	// ★칸 간격 (스플라인 거리 기준, uu).
	//   칸 i 의 거리 = HeadDistance - CarSpacing * i
	//   칸 메시 길이 + 원하는 틈만큼 설정한다. 너무 좁으면 곡선에서 칸끼리 겹쳐 보인다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Train", meta = (ClampMin = "1.0"))
	float CarSpacing = 400.f;

	// 선두 칸의 시작 거리.
	// ★CarSpacing * (칸 수 - 1) 이상이어야 출발 전에 뒤 칸들이 선로 시작점에 겹치지 않는다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Train")
	float StartDistanceOnTrack = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Movement, BlueprintReadOnly, Category = "S2|Train")
	FS2TrainMovement Movement;

	UFUNCTION()
	void OnRep_Movement();

	// 상태 전환 연출 (출발/급정거 사운드·진동)
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Train")
	void OnTrainStateChanged(ES2TrainState NewState);

private:
	// 칸 배열 검증 + 칸 번호 부여
	void InitializeCars();

	// 목표 도달 처리 (서버)
	void ArriveAtTarget();

	// 선두 거리로 전 칸을 각자의 거리에 배치한다 (곡선 순차 추종의 실제 구현부)
	void ApplyTransformAtDistance(float HeadDistance);

	// 현재 스플라인 거리 (서버/클라 각자 계산)
	float CurrentDistance = 0.f;

	// DepartTo 로 전달된 목표 인덱스 (도착 알림에 실어 보낸다). 서버 전용.
	int32 PendingTargetIndex = INDEX_NONE;
};
