// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameStateBase.h"
#include "Phase/DRPhaseBase.h"
#include "DRStageGameState.generated.h"

class ADRDoorManager;
class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;

// ������ ��ǥ ������Ʈ ��������Ʈ
DECLARE_MULTICAST_DELEGATE(FOnPhaseObjectiveChanged);
// Phase ���� ��������Ʈ
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChangedSignature, int32, NewPhaseIndex);
// ���̺� Ÿ�̸� ���� ������Ʈ ��������Ʈ
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWaveTimerChanged, int32 /*WaveNumber*/, float /*RemainingTime*/, bool /*bIsRestTime*/);
// 독가스 경고 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToxicGasWarningSignature, bool, bIsToxicGasWave);

// ������ ���� ������
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

    // DoorManager ���
    UFUNCTION(BlueprintCallable, Category = "Stage")
    void RegisterDoorManager(ADRDoorManager* InDoorManager);

    UFUNCTION(BlueprintPure, Category = "Stage")
    ADRDoorManager* GetDoorManager() const { return DoorManager; }

    // ========== ���� ���ø����̼� ==========
    UFUNCTION(BlueprintCallable, Category = "Phase")
    int32 GetCurrentPhaseIndex() const { return CurrentPhaseIndex; }

    UFUNCTION(BlueprintCallable, Category = "Phase")
    EPhaseState GetCurrentPhaseState() const { return CurrentPhaseState; }

    void SetCurrentPhaseIndex(int32 NewIndex);
    void SetCurrentPhaseState(EPhaseState NewState);

    // ========== Phase 1: Ŭ���� Ȯ�� ==========
    UFUNCTION(BlueprintCallable, Category = "Phase|Cleanser")
    bool IsCleanserAreaSecured() const { return bCleanserAreaSecured; }

    UFUNCTION(BlueprintCallable, Category = "Phase|Cleanser")
    int32 GetRemainingEnemiesInArea() const { return RemainingEnemiesInArea; }

    // ������ ������ Ȯ������ ��
    void SetCleanserAreaSecured(bool bSecured);
    // ���� óġ�� ������ ȣ�� (���� �� ��)
    void SetRemainingEnemiesInArea(int32 Count);

    // ========== Phase 2: ��ǰ ȸ�� ==========
    UFUNCTION(BlueprintCallable, Category = "Phase|Parts")
    int32 GetCollectedParts() const { return CollectedParts; }

    UFUNCTION(BlueprintCallable, Category = "Phase|Parts")
    bool IsCleanserActivated() const { return bCleanserActivated; }

    // ��ǰ�� ȹ���� ������ ȣ�� (ȹ���� ����)
    void SetCollectedParts(int32 Count);
    // Ŭ������ Ȱ��ȭ���� ��
    void SetCleanserActivated(bool bActivated);

    // ========== Phase 3: ��� ==========
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

    // �� ���̺� ���� �� (���� ���̺� ��ȣ)
    void SetCurrentWaveNumber(int32 WaveNumber);
    // �� ���̺� ���� �� (���� ���̺� ����)
    void SetCurrentWaveLevel(int32 WaveLevel);
    // �� ���̺� ��
    void SetTotalWaves(int32 Total);
    // Ŭ������ ������ ���� �� (Ŭ���� ���� ü��)
    void SetCleanserHealth(float Health);
    
    // ���̺� Ÿ�̸� ������Ʈ
    void SetWaveRemainingTime(float Time);
    void SetIsWaveRestTime(bool bIsRest);

    // 독가스 웨이브 여부
    void SetIsToxicGasWave(bool bIsToxicGas);

    UFUNCTION(BlueprintCallable, Category = "Phase|Defense")
    bool IsToxicGasWave() const { return bIsToxicGasWave; }

    // 독가스 경고 델리게이트 (모든 클라이언트에서 UI 바인딩용)
    UPROPERTY(BlueprintAssignable, Category = "Phase|Warning")
    FOnToxicGasWarningSignature OnToxicGasWarningDelegate;

    // ���̺� Ÿ�̸� ��������Ʈ
    FOnWaveTimerChanged OnWaveTimerChangedDelegate;

    // ========== Phase 4: ���� ==========
    UFUNCTION(BlueprintCallable, Category = "Phase|Boss")
    float GetBossHealth() const { return BossHealth; }

    // ������ ������ ���� �� (���� ���� ü��)
    void SetBossHealth(float Health);

    // Phase ��ǥ UI ����
    FOnPhaseObjectiveChanged OnPhaseObjectiveChangedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Phase")
    FOnPhaseChangedSignature OnPhaseChangedDelegate;
    
    void SetPhaseObjective(const FPhaseObjectiveData& ObjectiveData);
    void UpdatePhaseObjectiveProgress(int32 NewCount);

    FPhaseObjectiveData GetCurrentPhaseObjective() const { return CurrentPhaseObjective; }
    int32 GetCurrentObjectiveProgress() const { return CurrentObjectiveProgress; }

    UPROPERTY(ReplicatedUsing = OnRep_CurrentPhaseObjective)
    FPhaseObjectiveData CurrentPhaseObjective;

    // ========== 사운드 (Multicast RPC) ==========

    // 페이즈 시작 사운드 (모든 클라이언트에서 재생)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayPhaseStartSound();

    // 웨이브 시작 사운드 (모든 클라이언트에서 재생)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayWaveStartSound();

    // 게임 클리어 사운드 (모든 클라이언트에서 재생)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayGameClearSound();

    // 게임 오버 사운드 (모든 클라이언트에서 재생)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayGameOverSound();

    // 독가스 경고 사운드 (스폰 배치당 1회, 모든 클라이언트에서 재생)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayPoisonGasWarningSound();

    // 독가스 활성 루프 사운드 시작 (모든 클라이언트에서 2D 루프 재생)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_StartPoisonGasLoopSound();

    // 독가스 활성 루프 사운드 정지
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_StopPoisonGasLoopSound();

    // 서버 전용: 활성 독가스 카운트 관리 (PoisonGasActor가 호출)
    void NotifyPoisonGasActivated();
    void NotifyPoisonGasDeactivated();

    // ========== Phase3 스폰 포인트 VFX (Multicast RPC) ==========

    // 일반 적 스폰 포인트 VFX 활성화
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ActivateEnemySpawnPointVFX(const TArray<FTransform>& SpawnPointTransforms, UNiagaraSystem* NiagaraAsset);

    // 일반 적 스폰 포인트 VFX 비활성화
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_DeactivateEnemySpawnPointVFX();

    // 엘리트 보스 스폰 포인트 VFX 활성화
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ActivateEliteSpawnPointVFX(const TArray<FTransform>& SpawnPointTransforms, UNiagaraSystem* NiagaraAsset);

    // 엘리트 보스 스폰 포인트 VFX 비활성화
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_DeactivateEliteSpawnPointVFX();

protected:
    // ========== ���ø����̼� �ݹ� ==========
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

    UFUNCTION()
    void OnRep_IsToxicGasWave();

private:
    UPROPERTY()
    TArray<ADRCleanserSite*> CleanserSites;

    UPROPERTY()
    TObjectPtr<ADRDoorManager> DoorManager;

    // Phase1 ���� ������ �÷��̾� ��
    UPROPERTY()
    int32 InitialPlayerCount;
    
    // ========== ���� ���ø����̼� ������ ==========
    UPROPERTY(ReplicatedUsing = OnRep_CurrentPhaseIndex)
    int32 CurrentPhaseIndex;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentPhaseState)
    EPhaseState CurrentPhaseState;

    // ========== Phase 1: Ŭ���� Ȯ�� ==========
    UPROPERTY(Replicated)
    bool bCleanserAreaSecured;

    UPROPERTY(Replicated)
    int32 RemainingEnemiesInArea;

    // ========== Phase 2: ��ǰ ȸ�� ==========
    UPROPERTY(Replicated)
    int32 CollectedParts;

    UPROPERTY(Replicated)
    bool bCleanserActivated;

    // ========== Phase 3: ��� ==========
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

    UPROPERTY(ReplicatedUsing = OnRep_IsToxicGasWave)
    bool bIsToxicGasWave = false;

    // ========== Phase 4: ���� ==========
    UPROPERTY(Replicated)
    float BossHealth;

    // ========== UI ������Ʈ ==========
    UPROPERTY(ReplicatedUsing = OnRep_CurrentObjectiveProgress)
    int32 CurrentObjectiveProgress = 0;

    // ========== Phase3 VFX 컴포넌트 캐시 ==========
    UPROPERTY()
    TArray<TObjectPtr<UNiagaraComponent>> EnemySpawnPointVFXComponents;

    UPROPERTY()
    TArray<TObjectPtr<UNiagaraComponent>> EliteSpawnPointVFXComponents;

    // ========== 독가스 사운드 상태 ==========
    // 서버에서만 사용: 현재 활성 상태의 독가스 액터 수
    int32 ActivePoisonGasCount = 0;

    // 클라이언트/서버 각자 보유: 2D 루프 사운드 핸들 (1개만 유지)
    UPROPERTY()
    TObjectPtr<UAudioComponent> PoisonGasLoopAudioComponent;
};
