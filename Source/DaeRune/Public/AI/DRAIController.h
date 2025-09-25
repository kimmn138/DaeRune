// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DRAIController.generated.h"

class UBlackboardComponent;
class UBehaviorTreeComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;

/**
 * DREnemy 전용 AI 컨트롤러
 */
UCLASS()
class DAERUNE_API ADRAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	ADRAIController();

	// Team Interface
	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override { TeamId = NewTeamId; }
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	// 특정 거리 내 플레이어들 가져오기
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	TArray<AActor*> GetPlayers() const { return PerceivedPlayers; }

	// 감지된 플레이어 수
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	int32 GetPerceivedPlayerCount() const { return PerceivedPlayers.Num(); }

protected:
	// 비헤이비어 트리 실행 컴포넌트
	UPROPERTY()
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	// Perception 업데이트 콜백
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

private:
	FGenericTeamId TeamId;

	// 거리별로 정렬된 플레이어 배열
	UPROPERTY(VisibleAnywhere, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	TArray<AActor*> PerceivedPlayers;

	// 거리 업데이트
	void UpdatePlayer(AActor* Player);

	// 플레이어 제거
	void RemovePlayer(AActor* Player);
};
