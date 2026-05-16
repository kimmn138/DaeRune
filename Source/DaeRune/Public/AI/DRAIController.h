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
 * DREnemy ���� AI ��Ʈ�ѷ�
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

	// Ư�� �Ÿ� �� �÷��̾�� ��������
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	TArray<AActor*> GetPlayers() const { return PerceivedPlayers; }

	// ������ �÷��̾� ��
	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	int32 GetPerceivedPlayerCount() const { return PerceivedPlayers.Num(); }
	
	// ���� �ð� ������Ʈ
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void UpdateCombatTime();
        
    // ���� ��Ż ���� Ȯ��
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    bool HasCombatTimedOut(float TimeoutSeconds = 3.0f) const;

protected:
	// �����̺�� Ʈ�� ���� ������Ʈ
	UPROPERTY()
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	// Perception ������Ʈ �ݹ�
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

private:
	FGenericTeamId TeamId;

	// �Ÿ����� ���ĵ� �÷��̾� �迭
	UPROPERTY(VisibleAnywhere, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	TArray<AActor*> PerceivedPlayers;

	// �Ÿ� ������Ʈ
	void UpdatePlayer(AActor* Player);

	// �÷��̾� ����
	void RemovePlayer(AActor* Player);

	// [DEBUG] Phase3 멈춘 적 추적용 주기 로그
	FTimerHandle DebugStateLogTimerHandle;
	void DebugStateLog();
};
