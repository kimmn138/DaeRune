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
	
	// 전투 시간 업데이트
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void UpdateCombatTime();
        
    // 전투 이탈 여부 확인
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    bool HasCombatTimedOut(float TimeoutSeconds = 3.0f) const;

protected:
	// 비헤이비어 트리 실행 컴포넌트
	UPROPERTY()
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;

	virtual void BeginPlay() override;

	// 매 틱 ControlRotation 을 포커스로 맞추기 직전에 전투 대상 기본 포커스를 갱신한다
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;

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

	/**
	 * 전투 대상(블랙보드 TargetToFollow, 없으면 페이즈3 TargetCleanserSite)을 Default 우선순위 포커스로 건다.
	 * 이동 중에는 길찾기의 Move 포커스, 도발 · 첫 공격자 · BT 태스크는 Gameplay 포커스가 이긴다 —
	 * 그 둘이 없을 때(공격 사이 대기, 대상이 MoveTo 도달 반경 안에 있어 움직이지 않을 때) 대상 쪽으로 돈다.
	 * 예전에는 이때 걸린 포커스가 없어 ControlRotation 이 자기 방향에 머물렀고, 등 뒤에 붙은 플레이어를 두고 앞만 공격했다.
	 * 공격 · 스킬 중이거나 기절 · 사망이면 비운다 (ADREnemy::ShouldFaceCombatTarget)
	 */
	void UpdateCombatFacingFocus();
};
