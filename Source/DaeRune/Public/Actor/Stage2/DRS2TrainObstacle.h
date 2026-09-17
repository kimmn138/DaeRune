// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2TrainObstacle.generated.h"

class ADRS2TrainTrack;
class UArrowComponent;

/**
 * 선로 장애물 (Plan6 §4.9.5 / §14.6.3)
 *
 * ★두더지 보스가 장애물을 부수며 등장하는 사양이라, 장애물은
 *   "임계 도달 시 해제되는 잠금"이 아니라 "열차 정지 지점 정의 + 보스 등장 연출용 파괴 오브젝트"다.
 *
 * 전방 차단 역할은 ADRS2Barrier 가 담당한다 (장애물이 부서지면 막을 수 없으므로).
 *
 * ★배치 규약: **액터 위치는 정지 지점과 무관하다.**
 *   장애물 메시가 맵 통짜 익스포트라 피벗이 월드 원점에 구워져 있으면 액터를 (0,0,0) 에 둘 수밖에 없고,
 *   그러면 액터 위치는 선로와 아무 관계가 없어진다. 그래서 정지 지점은 별도 컴포넌트로 분리했다.
 *
 *   레벨에 배치한 뒤 인스턴스마다 이 둘을 옮긴다:
 *     · StopPoint      - 선로 위, 장애물 **앞** (= 열차 선두가 설 자리)
 *     · BossSpawnPoint - 보스가 튀어나올 자리
 *   둘 다 액터 루트에 붙어 있으므로 액터가 원점이면 원점에 남는다. 옮기지 않으면 로그로 에러가 뜬다.
 */
UCLASS()
class DAERUNE_API ADRS2TrainObstacle : public AActor
{
	GENERATED_BODY()

public:
	ADRS2TrainObstacle();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 트랙 상 정지 거리.
	//
	// ★미입력(0 이하)이면 **첫 호출 시점에** 위치를 스플라인에 투영해 계산한다 (지연 평가).
	//   BeginPlay 에서 계산하지 않는 이유: 장애물의 BeginPlay 는 맵 로드 시점에 끝나지만
	//   페이즈가 SetTrack() 으로 선로를 주입하는 것은 방6 페이즈 시작 시점이라 훨씬 나중이다.
	//   BeginPlay 에서만 계산하면 Track 이 아직 null 이라 -1 이 그대로 남고,
	//   DepartTo 가 이를 0 으로 클램프해 열차가 출발 즉시 "도착"해 버린다.
	UFUNCTION(BlueprintCallable, Category = "S2|Obstacle")
	float GetStopDistance();

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

	// ★열차 정지 지점. **이 컴포넌트를 선로 위, 장애물 앞으로 옮기는 것이 유일한 배치 작업이다.**
	//   여기를 스플라인에 투영한 거리가 곧 StopDistance 가 되며, 열차 선두가 정확히 이 자리에 선다
	//   (별도 여유 거리를 더하지 않는다 - 원하는 간격은 이 포인트를 놓는 위치로 표현한다).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Obstacle")
	TObjectPtr<USceneComponent> StopPoint;

#if WITH_EDITORONLY_DATA
	// 뷰포트에서 StopPoint 를 찾기 위한 표식 (게임에는 보이지 않는다)
	UPROPERTY()
	TObjectPtr<UArrowComponent> StopPointArrow;
#endif

	// 트랙 상 정지 거리. 0 이하면 StopPoint 위치로 자동 계산 (GetStopDistance 첫 호출 시).
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Obstacle")
	float StopDistance = -1.f;

	// StopDistance 가 미입력이면 지금 계산한다. 이미 값이 있거나 Track 이 없으면 아무것도 하지 않는다.
	void TryResolveStopDistance();

	// 선로 투영의 기준점 = StopPoint 의 월드 위치.
	// ★액터 위치가 아니다. 클래스 주석의 배치 규약 참조.
	FVector GetTrackReferenceLocation() const;

	// 정지 거리 확정 여부 (수동 입력 채택 또는 자동 계산 완료). 런타임 전용.
	bool bStopDistanceResolved = false;

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
