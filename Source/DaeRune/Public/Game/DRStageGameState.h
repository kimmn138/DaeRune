// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "DRStageGameState.generated.h"

// 페이즈 상태 열거형
UENUM(BlueprintType)
enum class EPhaseState : uint8
{
    NotStarted    UMETA(DisplayName = "Not Started"),
    InProgress    UMETA(DisplayName = "In Progress"),
    Completed     UMETA(DisplayName = "Completed"),
    Failed        UMETA(DisplayName = "Failed")
};

/**
 * 
 */
UCLASS()
class DAERUNE_API ADRStageGameState : public ADRGameStateBase
{
	GENERATED_BODY()
	
public:
    ADRStageGameState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ========== 상태 리플리케이션 ==========
    UFUNCTION(BlueprintCallable, Category = "Phase")
    int32 GetCurrentPhaseIndex() const { return CurrentPhaseIndex; }

    UFUNCTION(BlueprintCallable, Category = "Phase")
    EPhaseState GetCurrentPhaseState() const { return CurrentPhaseState; }

    void SetCurrentPhaseIndex(int32 NewIndex);
    void SetCurrentPhaseState(EPhaseState NewState);

    // ========== Phase 1: 클렌저 확보 ==========
    UFUNCTION(BlueprintCallable, Category = "Phase|Cleanser")
    bool IsCleanserAreaSecured() const { return bCleanserAreaSecured; }

    UFUNCTION(BlueprintCallable, Category = "Phase|Cleanser")
    int32 GetRemainingEnemiesInArea() const { return RemainingEnemiesInArea; }

    // 지역을 완전히 확보했을 때
    void SetCleanserAreaSecured(bool bSecured);
    // 적을 처치할 때마다 호출 (남은 적 수)
    void SetRemainingEnemiesInArea(int32 Count);

    // ========== Phase 2: 부품 회수 ==========
    UFUNCTION(BlueprintCallable, Category = "Phase|Parts")
    int32 GetCollectedParts() const { return CollectedParts; }

    UFUNCTION(BlueprintCallable, Category = "Phase|Parts")
    bool IsCleanserActivated() const { return bCleanserActivated; }

    // 부품을 획득할 때마다 호출 (획득한 갯수)
    void SetCollectedParts(int32 Count);
    // 클렌저를 활성화했을 때
    void SetCleanserActivated(bool bActivated);

    // ========== Phase 3: 방어 ==========
    UFUNCTION(BlueprintCallable, Category = "Phase|Defense")
    int32 GetCurrentWave() const { return CurrentWave; }

    UFUNCTION(BlueprintCallable, Category = "Phase|Defense")
    int32 GetTotalWaves() const { return TotalWaves; }

    UFUNCTION(BlueprintCallable, Category = "Phase|Defense")
    float GetCleanserHealth() const { return CleanserHealth; }

    // 새 웨이브 시작 시 (현재 웨이브 번호)
    void SetCurrentWave(int32 Wave);
    // 총 웨이브 수
    void SetTotalWaves(int32 Total);
    // 클렌저가 데미지 받을 때 (클렌저 남은 체력)
    void SetCleanserHealth(float Health);

    // ========== Phase 4: 보스 ==========
    UFUNCTION(BlueprintCallable, Category = "Phase|Boss")
    float GetBossHealth() const { return BossHealth; }

    // 보스가 데미지 받을 때 (현재 보스 체력)
    void SetBossHealth(float Health);

protected:
    // ========== 리플리케이션 콜백 ==========
    UFUNCTION()
    void OnRep_CurrentPhaseIndex();

    UFUNCTION()
    void OnRep_CurrentPhaseState();

private:
    // ========== 상태 리플리케이션 변수들 ==========
    UPROPERTY(ReplicatedUsing = OnRep_CurrentPhaseIndex)
    int32 CurrentPhaseIndex;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentPhaseState)
    EPhaseState CurrentPhaseState;

    // ========== Phase 1: 클렌저 확보 ==========
    UPROPERTY(Replicated)
    bool bCleanserAreaSecured;

    UPROPERTY(Replicated)
    int32 RemainingEnemiesInArea;

    // ========== Phase 2: 부품 회수 ==========
    UPROPERTY(Replicated)
    int32 CollectedParts;

    UPROPERTY(Replicated)
    bool bCleanserActivated;

    // ========== Phase 3: 방어 ==========
    UPROPERTY(Replicated)
    int32 CurrentWave;

    UPROPERTY(Replicated)
    int32 TotalWaves;

    UPROPERTY(Replicated)
    float CleanserHealth;

    // ========== Phase 4: 보스 ==========
    UPROPERTY(Replicated)
    float BossHealth;
};
