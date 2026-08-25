// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2TrainObstacle.generated.h"

class ADRS2TrainTrack;

/**
 * 선로 장애물 (Plan6 §4.9.5 / §14.6.3)
 *
 * ★두더지 보스가 장애물을 부수며 등장하는 사양이라, 장애물은
 *   "임계 도달 시 해제되는 잠금"이 아니라 "열차 정지 지점 정의 + 보스 등장 연출용 파괴 오브젝트"다.
 *
 * 전방 차단 역할은 ADRS2Barrier 가 담당한다 (장애물이 부서지면 막을 수 없으므로).
 */
UCLASS()
class DAERUNE_API ADRS2TrainObstacle : public AActor
{
	GENERATED_BODY()

public:
	ADRS2TrainObstacle();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 트랙 상 정지 거리. 미입력(음수)이면 BeginPlay 에서 위치를 스플라인에 투영해 자동 계산한다.
	UFUNCTION(BlueprintCallable, Category = "S2|Obstacle")
	float GetStopDistance() const { return StopDistance; }

	// 보스 등장 트랜스폼
	UFUNCTION(BlueprintCallable, Category = "S2|Obstacle")
	FTransform GetBossSpawnTransform() const;

	// 보스가 장애물을 부수며 등장 (서버). 등장과 동시에 호출된다.
	UFUNCTION(BlueprintCallable, Category = "S2|Obstacle")
	void BreakByBoss();

	UFUNCTION(BlueprintCallable, Category = "S2|Obstacle")
	bool IsBroken() const { return bBroken; }

	// 선로를 배선해 두면 StopDistance 자동 계산에 사용한다
	void SetTrack(ADRS2TrainTrack* InTrack) { Track = InTrack; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Obstacle")
	TObjectPtr<USceneComponent> SceneRoot;

	// 보스가 부수는 선로 차단물
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Obstacle")
	TObjectPtr<UStaticMeshComponent> ObstacleMesh;

	// 보스 등장 지점 (장애물 뒤/아래)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Obstacle")
	TObjectPtr<USceneComponent> BossSpawnPoint;

	// 트랙 상 정지 거리. 음수면 자동 계산.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Obstacle")
	float StopDistance = -1.f;

	// 열차가 이 장애물 앞에서 멈출 여유 거리 (장애물 직전에 세우기 위한 오프셋)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Obstacle", meta = (ClampMin = "0.0"))
	float StopMargin = 400.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Obstacle")
	TObjectPtr<ADRS2TrainTrack> Track;

	UPROPERTY(ReplicatedUsing = OnRep_bBroken, BlueprintReadOnly, Category = "S2|Obstacle")
	bool bBroken = false;

	UFUNCTION()
	void OnRep_bBroken();

	// 파편/먼지/사운드
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Obstacle")
	void OnBrokenVisual();
};
