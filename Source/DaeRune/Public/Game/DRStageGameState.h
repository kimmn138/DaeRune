// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameStateBase.h"
#include "Phase/DRPhaseBase.h"
#include "DRStageGameState.generated.h"

class ADRDoorManager;

// 페이즈 목표 업데이트 델리게이트
DECLARE_MULTICAST_DELEGATE(FOnPhaseObjectiveChanged);
// Phase 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChangedSignature, int32, NewPhaseIndex);
// 웨이브 타이머 정보 업데이트 델리게이트
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWaveTimerChanged, int32 /*WaveNumber*/, float /*RemainingTime*/, bool /*bIsRestTime*/);

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

    TArray<ADRCleanserSite*> GetCleanserSites() { return CleanserSites; }
    int32 GetInitialPlayerCount() { return InitialPlayerCount; }
    void SetCleanserSites(TArray<ADRCleanserSite*> InCleanserSites) { CleanserSites = InCleanserSites; }
    void SetInitialPlayerCount(float NewInitialPlayerCount) { InitialPlayerCount = NewInitialPlayerCount; }

    // DoorManager 등록
    UFUNCTION(BlueprintCallable, Category = "Stage")
    void RegisterDoorManager(ADRDoorManager* InDoorManager);

    UFUNCTION(BlueprintPure, Category = "Stage")
    ADRDoorManager* GetDoorManager() const { return DoorManager; }

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
    int32 GetCurrentWaveNumber() const { return CurrentWaveNumber; }

    UFUNCTION(BlueprintCallable, Category = "Phase|Defense")
    int32 GetCurrentWaveLevel() const { return CurrentWaveLevel; }

    UFUNCTION(BlueprintCallable, Category = "Phase|Defense")
    int32 GetTotalWaves() const { return TotalWaves; }

    UFUNCTION(BlueprintCallable, Category = "Phase|Defense")
    float GetCleanserHealth() const { return CleanserHealth; }
    
    UFUNCTION(BlueprintCallable, Category = "Phase|Defense")
    float GetWaveRemainingTime() const { return WaveRemainingTime; }
    
    UFUNCTION(BlueprintCallable, Category = "Phase|Defense")
    bool IsWaveRestTime() const { return bIsWaveRestTime; }

    // 새 웨이브 시작 시 (현재 웨이브 번호)
    void SetCurrentWaveNumber(int32 WaveNumber);
    // 새 웨이브 레벨 시 (현재 웨이브 레벨)
    void SetCurrentWaveLevel(int32 WaveLevel);
    // 총 웨이브 수
    void SetTotalWaves(int32 Total);
    // 클렌저가 데미지 받을 때 (클렌저 남은 체력)
    void SetCleanserHealth(float Health);
    
    // 웨이브 타이머 업데이트
    void SetWaveRemainingTime(float Time);
    void SetIsWaveRestTime(bool bIsRest);
    
    // 웨이브 타이머 델리게이트
    FOnWaveTimerChanged OnWaveTimerChangedDelegate;

    // ========== Phase 4: 보스 ==========
    UFUNCTION(BlueprintCallable, Category = "Phase|Boss")
    float GetBossHealth() const { return BossHealth; }

    // 보스가 데미지 받을 때 (현재 보스 체력)
    void SetBossHealth(float Health);

    // Phase 목표 UI 관련
    FOnPhaseObjectiveChanged OnPhaseObjectiveChangedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Phase")
    FOnPhaseChangedSignature OnPhaseChangedDelegate;
    
    void SetPhaseObjective(const FPhaseObjectiveData& ObjectiveData);
    void UpdatePhaseObjectiveProgress(int32 NewCount);
    
    FPhaseObjectiveData GetCurrentPhaseObjective() const { return CurrentPhaseObjective; }
    int32 GetCurrentObjectiveProgress() const { return CurrentObjectiveProgress; }

    UPROPERTY(ReplicatedUsing = OnRep_CurrentPhaseObjective)
    FPhaseObjectiveData CurrentPhaseObjective;

protected:
    // ========== 리플리케이션 콜백 ==========
    UFUNCTION()
    void OnRep_CurrentPhaseIndex();

    UFUNCTION()
    void OnRep_CurrentPhaseState();

    UFUNCTION()
    void OnRep_CurrentPhaseObjective();
    
    UFUNCTION()
    void OnRep_CurrentObjectiveProgress();

    UFUNCTION()
    void OnRep_WaveRemainingTime();

    UFUNCTION()
    void OnRep_IsWaveRestTime();

private:
    UPROPERTY()
    TArray<ADRCleanserSite*> CleanserSites;

    UPROPERTY()
    TObjectPtr<ADRDoorManager> DoorManager;

    // Phase1 시작 시점의 플레이어 수
    UPROPERTY()
    int32 InitialPlayerCount;
    
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
    int32 CurrentWaveNumber;

    UPROPERTY(Replicated)
    int32 CurrentWaveLevel;

    UPROPERTY(Replicated)
    int32 TotalWaves;

    UPROPERTY(Replicated)
    float CleanserHealth;
    
    UPROPERTY(ReplicatedUsing = OnRep_WaveRemainingTime)
    float WaveRemainingTime;

    UPROPERTY(ReplicatedUsing = OnRep_IsWaveRestTime)
    bool bIsWaveRestTime;

    // ========== Phase 4: 보스 ==========
    UPROPERTY(Replicated)
    float BossHealth;

    // ========== UI 업데이트 ==========
    UPROPERTY(ReplicatedUsing = OnRep_CurrentObjectiveProgress)
    int32 CurrentObjectiveProgress = 0;
};
