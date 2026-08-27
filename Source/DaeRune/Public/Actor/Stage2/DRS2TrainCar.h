// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DRInteractable.h"
#include "DRS2TrainCar.generated.h"

class UWidgetComponent;
class ADRCharacter;
class ADRS2Train;
class ADRS2TrainTrack;

/**
 * 열차 칸 (바구니) — Plan6 §14.6.2
 *
 * ★설계 변경 (2026-08-07): 당초 "열차 액터 + 좌석 액터" 2계층이었으나,
 *   실제 아트가 좌석 없는 바구니 형태의 1칸짜리 열차라서 **칸 자체가 탑승 지점**이 되었다.
 *   별도 좌석 액터를 두지 않고 이 칸 하나가 메시·탑승·하차를 모두 담당한다.
 *
 * ★곡선 추종: 칸마다 **자기 스플라인 거리**를 갖는다.
 *     자기 거리 = HeadDistance - CarSpacing * CarIndex
 *   선두 칸(Index 0)이 먼저 코너에 진입해 꺾이고, 뒤 칸은 나중에 같은 지점을 지나며 꺾인다.
 *   4칸이 한 덩어리로 회전하지 않는다.
 *
 * 이동은 열차(ADRS2Train)의 Tick 이 서버·클라 양쪽에서 UpdateFromTrack 을 호출해 만든다.
 * 칸 자신은 이동을 복제하지 않는다("1회 복제 + 로컬 시뮬" 모델).
 */
UCLASS()
class DAERUNE_API ADRS2TrainCar : public AActor, public IDRInteractable
{
	GENERATED_BODY()

public:
	ADRS2TrainCar();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ========== 선로 추종 ==========

	// 열차가 매 프레임 호출한다. 자기 칸 번호만큼 뒤처진 거리의 스플라인 트랜스폼을 적용한다.
	void UpdateFromTrack(const ADRS2TrainTrack* Track, float HeadDistance, float CarSpacing);

	// 칸 번호 (0 = 선두). 열차가 배열 순서로 자동 지정한다.
	int32 GetCarIndex() const { return CarIndex; }
	void SetCarIndex(int32 InIndex) { CarIndex = InIndex; }

	// ========== 탑승 ==========

	// 탑승 가능 여부. 마운트 시스템의 CanBeMountedBy 검증 항목을 미러링한다.
	UFUNCTION(BlueprintCallable, Category = "S2|TrainCar")
	bool CanBeBoardedBy(const ADRCharacter* Candidate) const;

	// 탑승 처리 (서버)
	void Board(ADRCharacter* Character);

	// 하차 처리 (서버). ExitPoint 로 내려놓는다.
	void Deboard();

	UFUNCTION(BlueprintCallable, Category = "S2|TrainCar")
	ADRCharacter* GetRider() const { return Rider; }

	UFUNCTION(BlueprintCallable, Category = "S2|TrainCar")
	bool IsOccupied() const { return Rider != nullptr; }

	// 탑승자가 attach 될 지점 (바구니 안)
	USceneComponent* GetRiderAttachPoint() const { return RiderAttachPoint; }

	void SetOwningTrain(ADRS2Train* InTrain) { OwningTrain = InTrain; }
	ADRS2Train* GetOwningTrain() const { return OwningTrain; }

	// IDRInteractable
	virtual void SetInteractionUIVisible(bool bShow) override;

protected:
	virtual void BeginPlay() override;

	// 바구니 메시. 이것이 곧 탑승 지점이며 시점 라인트레이스 대상이다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|TrainCar")
	TObjectPtr<UStaticMeshComponent> CarMesh;

	// 탑승자 attach 위치 (바구니 안쪽 바닥)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|TrainCar")
	TObjectPtr<USceneComponent> RiderAttachPoint;

	// 하차 시 내려놓을 위치 (칸 옆 바닥)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|TrainCar")
	TObjectPtr<USceneComponent> ExitPoint;

	// "F 탑승" 프롬프트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|TrainCar")
	TObjectPtr<UWidgetComponent> InteractionWidget;

	// 칸 번호. 열차의 Cars 배열 순서로 자동 지정되므로 수동 입력은 불필요하다(디버그 표시용).
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "S2|TrainCar")
	int32 CarIndex = 0;

	// 선로 위로 띄울 높이. 메시 피벗이 바닥 중앙이 아니면 이 값으로 보정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|TrainCar")
	float HeightOffset = 0.f;

	// 스플라인의 피치/롤(경사·뱅킹)을 무시하고 수평을 유지할지.
	// 바구니 열차는 수평 유지가 자연스럽고, 탑승자가 기울어 벽에 끼는 것도 막는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|TrainCar")
	bool bLevelPitchAndRoll = true;

	// 소유 열차. 비워두면 열차가 배선하거나 BeginPlay 에서 부모로 추론한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|TrainCar")
	TObjectPtr<ADRS2Train> OwningTrain;

	UPROPERTY(ReplicatedUsing = OnRep_Rider, BlueprintReadOnly, Category = "S2|TrainCar")
	TObjectPtr<ADRCharacter> Rider;

	UFUNCTION()
	void OnRep_Rider();

	// 탑승/하차 연출
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|TrainCar")
	void OnRiderChanged(ADRCharacter* NewRider);

private:
	// 탑승자가 죽으면 칸을 자동으로 비운다
	UFUNCTION()
	void HandleRiderDeath(AActor* DeadActor);
};
