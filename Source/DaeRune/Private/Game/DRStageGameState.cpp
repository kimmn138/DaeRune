// Copyright DaeRune


#include "Game/DRStageGameState.h"

ADRStageGameState::ADRStageGameState()
{
    // 초기값 설정
    CurrentPhaseIndex = -1;
    CurrentPhaseState = EPhaseState::NotStarted;

    // Phase 1
    bCleanserAreaSecured = false;
    RemainingEnemiesInArea = 0;

    // Phase 2
    CollectedParts = 0;
    bCleanserActivated = false;

    // Phase 3
    CurrentWave = 0;
    TotalWaves = 5; // 기본값
    CleanserHealth = 1000.0f;

    // Phase 4
    BossHealth = 1000.0f;
}

void ADRStageGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 상태 리플리케이션
    DOREPLIFETIME(ADRStageGameState, CurrentPhaseIndex);
    DOREPLIFETIME(ADRStageGameState, CurrentPhaseState);

    // Phase 1
    DOREPLIFETIME(ADRStageGameState, bCleanserAreaSecured);
    DOREPLIFETIME(ADRStageGameState, RemainingEnemiesInArea);

    // Phase 2
    DOREPLIFETIME(ADRStageGameState, CollectedParts);
    DOREPLIFETIME(ADRStageGameState, bCleanserActivated);

    // Phase 3
    DOREPLIFETIME(ADRStageGameState, CurrentWave);
    DOREPLIFETIME(ADRStageGameState, TotalWaves);
    DOREPLIFETIME(ADRStageGameState, CleanserHealth);

    // Phase 4
    DOREPLIFETIME(ADRStageGameState, BossHealth);
}

void ADRStageGameState::SetCurrentPhaseIndex(int32 NewIndex)
{
    if (HasAuthority())
    {
        CurrentPhaseIndex = NewIndex;
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

void ADRStageGameState::SetCurrentWave(int32 Wave)
{
    if (HasAuthority())
    {
        CurrentWave = FMath::Max(0, Wave);
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
    }   
}

void ADRStageGameState::SetBossHealth(float Health)
{
    if (HasAuthority())
    {
        BossHealth = FMath::Clamp(Health, 0.0f, 1000.0f);
    }
}

void ADRStageGameState::OnRep_CurrentPhaseIndex()
{
    // 클라이언트 UI 업데이트
}

void ADRStageGameState::OnRep_CurrentPhaseState()
{
    // 클라이언트 상태 변경 알림
    FString StateString;
    switch (CurrentPhaseState)
    {
    case EPhaseState::NotStarted:
        StateString = "Not Started";
        break;
    case EPhaseState::InProgress:
        StateString = "In Progress";
        break;
    case EPhaseState::Completed:
        StateString = "Completed";
        break;
    case EPhaseState::Failed:
        StateString = "Failed";
        break;
    }
}
