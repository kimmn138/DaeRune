// Copyright DaeRune


#include "Game/DRStageGameState.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/CombatInterface.h"
#include "Net/UnrealNetwork.h"
#include "Actor/DRDoorManager.h"

ADRStageGameState::ADRStageGameState()
{
    // �ʱⰪ ����
    CurrentPhaseIndex = -1;
    CurrentPhaseState = EPhaseState::NotStarted;

    // Phase 1
    bCleanserAreaSecured = false;
    RemainingEnemiesInArea = 0;

    // Phase 2
    CollectedParts = 0;
    bCleanserActivated = false;

    // Phase 3
    CurrentWaveNumber = 0;
    CurrentWaveLevel = 0;
    TotalWaves = 5; // �⺻��
    CleanserHealth = 1000.0f;
    WaveRemainingTime = 0.0f;
    bIsWaveRestTime = false;

    // Phase 4
    BossHealth = 1000.0f;

    CurrentPhaseObjective.PhaseNumber = 0;  // 0은 "준비 중" 의미
    CurrentPhaseObjective.ObjectiveTitle = FText::FromString("Preparing...");
    CurrentPhaseObjective.ProgressFormat = FText::FromString("Progress");
    CurrentPhaseObjective.RequiredCount = 0;
}

void ADRStageGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // ���� ���ø����̼�
    DOREPLIFETIME(ADRStageGameState, CurrentPhaseIndex);
    DOREPLIFETIME(ADRStageGameState, CurrentPhaseState);

    // Phase 1
    DOREPLIFETIME(ADRStageGameState, bCleanserAreaSecured);
    DOREPLIFETIME(ADRStageGameState, RemainingEnemiesInArea);

    // Phase 2
    DOREPLIFETIME(ADRStageGameState, CollectedParts);
    DOREPLIFETIME(ADRStageGameState, bCleanserActivated);

    // Phase 3
    DOREPLIFETIME(ADRStageGameState, CurrentWaveNumber);
    DOREPLIFETIME(ADRStageGameState, CurrentWaveLevel);
    DOREPLIFETIME(ADRStageGameState, TotalWaves);
    DOREPLIFETIME(ADRStageGameState, CleanserHealth);
    DOREPLIFETIME(ADRStageGameState, WaveRemainingTime);
    DOREPLIFETIME(ADRStageGameState, bIsWaveRestTime);

    // Phase 4
    DOREPLIFETIME(ADRStageGameState, BossHealth);

    // UI 업데이트
    DOREPLIFETIME(ADRStageGameState, CurrentPhaseObjective);
    DOREPLIFETIME(ADRStageGameState, CurrentObjectiveProgress);
}

void ADRStageGameState::RegisterDoorManager(ADRDoorManager* InDoorManager)
{
    if (!InDoorManager) return;

    DoorManager = InDoorManager;
}

void ADRStageGameState::SetCurrentPhaseIndex(int32 NewIndex)
{
    if (HasAuthority())
    {
        CurrentPhaseIndex = NewIndex;

        // Phase 변경 델리게이트 브로드캐스트
        OnPhaseChangedDelegate.Broadcast(NewIndex);
    }
}

void ADRStageGameState::SetCurrentPhaseState(EPhaseState NewState)
{
    if (HasAuthority())
    {
        CurrentPhaseState = NewState;
    }
}

void ADRStageGameState::SetCleanserAreaSecured(bool bSecured)
{
    if (HasAuthority())
    {
        bCleanserAreaSecured = bSecured;
    }
}

void ADRStageGameState::SetRemainingEnemiesInArea(int32 Count)
{
    if (HasAuthority())
    {
        RemainingEnemiesInArea = FMath::Max(0, Count);
    }
}

void ADRStageGameState::SetCollectedParts(int32 Count)
{
    if (HasAuthority())
    {
        CollectedParts = FMath::Clamp(Count, 0, 4);
    }
}

void ADRStageGameState::SetCleanserActivated(bool bActivated)
{
    if (HasAuthority())
    {
        bCleanserActivated = bActivated;
    }
}

void ADRStageGameState::SetCurrentWaveNumber(int32 WaveNumber)
{
    if (HasAuthority())
    {
        CurrentWaveNumber = FMath::Max(0, WaveNumber);
    }
}

void ADRStageGameState::SetCurrentWaveLevel(int32 WaveLevel)
{
    if (HasAuthority())
    {
        CurrentWaveLevel = FMath::Max(0, WaveLevel);
    }
}

void ADRStageGameState::SetTotalWaves(int32 Total)
{
    if (HasAuthority())
    {
        TotalWaves = FMath::Max(1, Total);
    }
}

void ADRStageGameState::SetCleanserHealth(float Health)
{
    if (HasAuthority())
    {
        CleanserHealth = FMath::Clamp(Health, 0.0f, 1000.0f);

        // Ŭ���� ü���� 0�� �Ǹ� ���� ó��
        if (CleanserHealth <= 0.0f)
        {
            SetCurrentPhaseState(EPhaseState::Failed);
        }
    }   
}

void ADRStageGameState::SetWaveRemainingTime(float Time)
{
    if (HasAuthority())
    {
        WaveRemainingTime = FMath::Max(0.0f, Time);
        // 서버에서만 직접 브로드캐스트
        OnWaveTimerChangedDelegate.Broadcast(CurrentWaveNumber, WaveRemainingTime, bIsWaveRestTime);
    }
}

void ADRStageGameState::SetIsWaveRestTime(bool bIsRest)
{
    if (HasAuthority())
    {
        bIsWaveRestTime = bIsRest;
        // 서버에서만 직접 브로드캐스트
        OnWaveTimerChangedDelegate.Broadcast(CurrentWaveNumber, WaveRemainingTime, bIsWaveRestTime);
    }
}

void ADRStageGameState::SetBossHealth(float Health)
{
    if (HasAuthority())
    {
        BossHealth = FMath::Clamp(Health, 0.0f, 1000.0f);
    }
}

void ADRStageGameState::SetPhaseObjective(const FPhaseObjectiveData& ObjectiveData)
{
    if (!HasAuthority()) return;
    
    CurrentPhaseObjective = ObjectiveData;
    CurrentObjectiveProgress = 0;
    OnPhaseObjectiveChangedDelegate.Broadcast();
}

void ADRStageGameState::UpdatePhaseObjectiveProgress(int32 NewCount)
{
    if (!HasAuthority()) return;
    
    CurrentObjectiveProgress = FMath::Clamp(NewCount, 0, CurrentPhaseObjective.RequiredCount);
    OnPhaseObjectiveChangedDelegate.Broadcast();
}

void ADRStageGameState::OnRep_CurrentPhaseIndex()
{
    OnPhaseChangedDelegate.Broadcast(CurrentPhaseIndex);
}

void ADRStageGameState::OnRep_CurrentPhaseObjective()
{
    OnPhaseObjectiveChangedDelegate.Broadcast();
}

void ADRStageGameState::OnRep_CurrentPhaseState()
{
    // 클라이언트에서 Phase 상태 변경 시 필요한 처리
    // 현재는 별도 처리 없음
}

void ADRStageGameState::OnRep_CurrentObjectiveProgress()
{
    OnPhaseObjectiveChangedDelegate.Broadcast();
}

void ADRStageGameState::OnRep_WaveRemainingTime()
{
    // 클라이언트에서 Replicated 변수 변경 시 델리게이트 브로드캐스트
    OnWaveTimerChangedDelegate.Broadcast(CurrentWaveNumber, WaveRemainingTime, bIsWaveRestTime);
}

void ADRStageGameState::OnRep_IsWaveRestTime()
{
    // 클라이언트에서 Replicated 변수 변경 시 델리게이트 브로드캐스트
    OnWaveTimerChangedDelegate.Broadcast(CurrentWaveNumber, WaveRemainingTime, bIsWaveRestTime);
}
