// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameStateBase.h"
#include "Phase/DRPhaseBase.h"
#include "Engine/NetSerialization.h"
#include "DRStageGameState.generated.h"

class ADRDoorManager;
class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UDRSoundDataAsset;
class USoundBase;

// 페이즈 목표 업데이트 델리게이트
DECLARE_MULTICAST_DELEGATE(FOnPhaseObjectiveChanged);
// Phase 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChangedSignature, int32, NewPhaseIndex);
// 웨이브 타이머 정보 업데이트 델리게이트
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWaveTimerChanged, int32 /*WaveNumber*/, float /*RemainingTime*/, bool /*bIsRestTime*/);
// 독가스 경고 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToxicGasWarningSignature, bool, bIsToxicGasWave);
// 목표 클리어 델리게이트 (Multicast RPC로 서버/클라 공통 발화)
DECLARE_MULTICAST_DELEGATE(FOnObjectiveCompleted);
// 웨이브 방어 UI(웨이브 타이머 + 클렌저 사이트 HP) 표시 여부 델리게이트 (Plan6 §5.4)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveDefenseUIActiveSignature, bool, bIsActive);

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

    const TArray<TObjectPtr<ADRCleanserSite>>& GetCleanserSites() const { return CleanserSites; }
    int32 GetInitialPlayerCount() const { return InitialPlayerCount; }
    void SetCleanserSites(const TArray<ADRCleanserSite*>& InCleanserSites)
    {
        CleanserSites.Reset(InCleanserSites.Num());
        for (ADRCleanserSite* Site : InCleanserSites)
        {
            CleanserSites.Add(Site);
        }
    }
    void SetInitialPlayerCount(int32 NewInitialPlayerCount) { InitialPlayerCount = NewInitialPlayerCount; }

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
    float GetWaveRemainingTime() const { return FMath::Max(0.0f, static_cast<float>(WaveTimerEndServerTime - GetServerWorldTimeSeconds())); }
    
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
    // 웨이브/휴식 타이머 시작 - 종료 시각만 1회 복제하고 남은 시간은 각 머신이 로컬 계산
    void StartWaveTimer(float DurationSeconds);
    void SetIsWaveRestTime(bool bIsRest);

    // 독가스 웨이브 여부
    void SetIsToxicGasWave(bool bIsToxicGas);

    UFUNCTION(BlueprintCallable, Category = "Phase|Defense")
    bool IsToxicGasWave() const { return bIsToxicGasWave; }

    // 독가스 경고 델리게이트 (모든 클라이언트에서 UI 바인딩용)
    UPROPERTY(BlueprintAssignable, Category = "Phase|Warning")
    FOnToxicGasWarningSignature OnToxicGasWarningDelegate;

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

    // 목표 클리어 알림 (서버에서 호출 - 1회성 주요 연출이므로 Reliable)
    // 진행도 복제와 달리 클리어 순간을 클라이언트가 확실히 수신하도록 명시적 RPC 사용
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ObjectiveCompleted();

    // 목표 클리어 델리게이트 (위젯 컨트롤러가 바인딩)
    FOnObjectiveCompleted OnObjectiveCompletedDelegate;

    FPhaseObjectiveData GetCurrentPhaseObjective() const { return CurrentPhaseObjective; }
    int32 GetCurrentObjectiveProgress() const { return CurrentObjectiveProgress; }

    UPROPERTY(ReplicatedUsing = OnRep_CurrentPhaseObjective)
    FPhaseObjectiveData CurrentPhaseObjective;

    // ========== 웨이브 방어 UI 표시 플래그 (Plan6 §5.4) ==========
    // 웨이브 타이머 UI + 클렌저 사이트 HP UI 를 띄울지 여부.
    // 기존에는 OverlayWidgetController가 "페이즈 인덱스 == 2"로 판단했으나,
    // 스테이지2에서는 인덱스 2가 다른 페이즈라 잘못된 UI가 표시되는 문제가 있었다.
    // 이 UI를 사용하는 페이즈(UDRPhase3)가 직접 켜고 끈다.
    UFUNCTION(BlueprintCallable, Category = "Phase|UI")
    void SetWaveDefenseUIActive(bool bActive);

    UFUNCTION(BlueprintCallable, Category = "Phase|UI")
    bool IsWaveDefenseUIActive() const { return bWaveDefenseUIActive; }

    UPROPERTY(BlueprintAssignable, Category = "Phase|UI")
    FOnWaveDefenseUIActiveSignature OnWaveDefenseUIActiveChangedDelegate;

    // ========== 사운드 (Multicast RPC) ==========

    // 페이즈 시작 사운드 (1회성 주요 연출 - 유실 시 다시 재생할 기회가 없으므로 Reliable)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayPhaseStartSound();

    // 웨이브 시작 사운드 (주기 반복 코스메틱 - 유실돼도 무방하므로 Unreliable)
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayWaveStartSound();

    // 게임 클리어 사운드 (1회성 주요 연출 - 유실 시 다시 재생할 기회가 없으므로 Reliable)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayGameClearSound();

    // 게임 오버 사운드 (1회성 주요 연출 - 유실 시 다시 재생할 기회가 없으므로 Reliable)
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayGameOverSound();

    // 독가스 경고 사운드 (주기 반복 코스메틱 - 유실돼도 무방하므로 Unreliable)
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayPoisonGasWarningSound();

    // 서버 전용: 활성 독가스 카운트 관리 (PoisonGasActor가 호출)
    // 루프 사운드는 복제 프로퍼티(bPoisonGasLoopActive) 기반이라 유실/레이트 조인에도 상태가 어긋나지 않음
    void NotifyPoisonGasActivated();
    void NotifyPoisonGasDeactivated();

    // ========== Phase3 스폰 포인트 VFX (복제 프로퍼티 - 유실/레이트 조인에도 안전) ==========

    // 일반 적 스폰 포인트 VFX 활성화 (서버 전용, 위치 배열은 양자화 벡터로 1회만 복제)
    void SetEnemySpawnPointVFX(const TArray<FVector_NetQuantize>& Locations, UNiagaraSystem* NiagaraAsset);

    // 일반 적 스폰 포인트 VFX 비활성화 (서버 전용)
    void ClearEnemySpawnPointVFX();

    // 엘리트 보스 스폰 포인트 VFX 활성화 (서버 전용)
    void SetEliteSpawnPointVFX(const TArray<FVector_NetQuantize>& Locations, UNiagaraSystem* NiagaraAsset);

    // 엘리트 보스 스폰 포인트 VFX 비활성화 (서버 전용)
    void ClearEliteSpawnPointVFX();

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
    void OnRep_WaveDefenseUIActive();

    UFUNCTION()
    void OnRep_WaveTimerEndServerTime();

    // 웨이브 타이머 UI 갱신용 로컬 1초 틱 (서버/클라 공통, 복제 트래픽 없음)
    void StartLocalWaveTimerTick();
    void BroadcastWaveTimerTick();

    FTimerHandle WaveTimerLocalTickHandle;

    UFUNCTION()
    void OnRep_IsWaveRestTime();

    UFUNCTION()
    void OnRep_IsToxicGasWave();

    UFUNCTION()
    void OnRep_PoisonGasLoopActive();

    UFUNCTION()
    void OnRep_EnemySpawnPointVFXLocations();

    UFUNCTION()
    void OnRep_EliteSpawnPointVFXLocations();

    // 로컬(서버/클라 공통) 코스메틱 상태 반영
    void UpdatePoisonGasLoopSound();
    void RefreshEnemySpawnPointVFX();
    void RefreshEliteSpawnPointVFX();

    // SoundDataAsset의 지정 사운드를 2D로 재생하는 공용 헬퍼
    // (AssetManager → SoundData → 널체크 보일러플레이트 제거)
    void PlaySound2DFromSoundData(TObjectPtr<USoundBase> UDRSoundDataAsset::* SoundMember);

private:
    UPROPERTY()
    TArray<TObjectPtr<ADRCleanserSite>> CleanserSites;

    UPROPERTY()
    TObjectPtr<ADRDoorManager> DoorManager;

    // Phase1 시작 시점의 플레이어 수
    UPROPERTY()
    int32 InitialPlayerCount = 0;
    
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
    
    // 현재 웨이브/휴식 타이머의 종료 시각 (서버 월드 시간 기준, 웨이브당 1회만 복제)
    UPROPERTY(ReplicatedUsing = OnRep_WaveTimerEndServerTime)
    float WaveTimerEndServerTime;

    UPROPERTY(ReplicatedUsing = OnRep_IsWaveRestTime)
    bool bIsWaveRestTime;

    UPROPERTY(ReplicatedUsing = OnRep_IsToxicGasWave)
    bool bIsToxicGasWave = false;

    // 독가스 활성 루프 사운드 상태 (RPC 대신 복제 프로퍼티 - 유실/레이트 조인에도 안전)
    UPROPERTY(ReplicatedUsing = OnRep_PoisonGasLoopActive)
    bool bPoisonGasLoopActive = false;

    // ========== Phase 4: 보스 ==========
    UPROPERTY(Replicated)
    float BossHealth;

    // ========== UI 업데이트 ==========
    UPROPERTY(ReplicatedUsing = OnRep_CurrentObjectiveProgress)
    int32 CurrentObjectiveProgress = 0;

    // 웨이브 방어 UI(타이머 + 클렌저 HP) 표시 여부 (Plan6 §5.4)
    UPROPERTY(ReplicatedUsing = OnRep_WaveDefenseUIActive)
    bool bWaveDefenseUIActive = false;

    // ========== Phase3 스폰 포인트 VFX 상태 (복제) ==========
    // 빈 배열 = VFX 꺼짐. VFX 끝점이라 양자화 벡터로 충분
    UPROPERTY(ReplicatedUsing = OnRep_EnemySpawnPointVFXLocations)
    TArray<FVector_NetQuantize> EnemySpawnPointVFXLocations;

    UPROPERTY(Replicated)
    TObjectPtr<UNiagaraSystem> EnemySpawnPointVFXAsset;

    UPROPERTY(ReplicatedUsing = OnRep_EliteSpawnPointVFXLocations)
    TArray<FVector_NetQuantize> EliteSpawnPointVFXLocations;

    UPROPERTY(Replicated)
    TObjectPtr<UNiagaraSystem> EliteSpawnPointVFXAsset;

    // ========== Phase3 VFX 컴포넌트 캐시 (로컬) ==========
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
