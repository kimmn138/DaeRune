// Copyright DaeRune


#include "Game/DRStageGameState.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/CombatInterface.h"
#include "Net/UnrealNetwork.h"
#include "Actor/DRDoorManager.h"
#include "Sound/DRSoundManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/DRSoundDataAsset.h"
#include "DRAssetManager.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "Actor/DRBGMActor.h"

#define LOCTEXT_NAMESPACE "DRPhase"

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
    WaveTimerEndServerTime = 0.0f;
    bIsWaveRestTime = false;
    bIsToxicGasWave = false;

    // Phase 4
    BossHealth = 1000.0f;

    CurrentPhaseObjective.PhaseNumber = 0;  // 0은 "준비 중" 의미
    CurrentPhaseObjective.ObjectiveTitle = LOCTEXT("Phase_PreparingTitle", "준비 중...");
    CurrentPhaseObjective.ProgressFormat = LOCTEXT("Phase_DefaultProgress", "진행도");
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
    DOREPLIFETIME(ADRStageGameState, WaveTimerEndServerTime);
    DOREPLIFETIME(ADRStageGameState, bIsWaveRestTime);
    DOREPLIFETIME(ADRStageGameState, bIsToxicGasWave);
    DOREPLIFETIME(ADRStageGameState, bPoisonGasLoopActive);
    DOREPLIFETIME(ADRStageGameState, bWaveDefenseUIActive);
    DOREPLIFETIME(ADRStageGameState, EnemySpawnPointVFXLocations);
    DOREPLIFETIME(ADRStageGameState, EnemySpawnPointVFXAsset);
    DOREPLIFETIME(ADRStageGameState, EliteSpawnPointVFXLocations);
    DOREPLIFETIME(ADRStageGameState, EliteSpawnPointVFXAsset);

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

void ADRStageGameState::StartWaveTimer(float DurationSeconds)
{
    if (HasAuthority())
    {
        // 종료 시각만 1회 복제 - 남은 시간은 각 머신이 서버 월드 시간으로 로컬 계산
        WaveTimerEndServerTime = static_cast<float>(GetServerWorldTimeSeconds()) + FMath::Max(0.0f, DurationSeconds);
        StartLocalWaveTimerTick();
    }
}

void ADRStageGameState::SetIsWaveRestTime(bool bIsRest)
{
    if (HasAuthority())
    {
        bIsWaveRestTime = bIsRest;
        // 서버에서만 직접 브로드캐스트
        OnWaveTimerChangedDelegate.Broadcast(CurrentWaveNumber, GetWaveRemainingTime(), bIsWaveRestTime);
    }
}

void ADRStageGameState::SetIsToxicGasWave(bool bIsToxicGas)
{
    if (HasAuthority())
    {
        bIsToxicGasWave = bIsToxicGas;
        // 서버에서 즉시 브로드캐스트 (리슨 서버 플레이어용)
        OnToxicGasWarningDelegate.Broadcast(bIsToxicGas);
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

void ADRStageGameState::Multicast_ObjectiveCompleted_Implementation()
{
    OnObjectiveCompletedDelegate.Broadcast();
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

void ADRStageGameState::SetWaveDefenseUIActive(bool bActive)
{
    // 서버 전용. 값이 같으면 아무것도 하지 않는다(멱등).
    if (!HasAuthority() || bWaveDefenseUIActive == bActive) return;

    bWaveDefenseUIActive = bActive;

    // 리슨 서버에서도 UI가 갱신되도록 RepNotify 수동 호출
    OnRep_WaveDefenseUIActive();
}

void ADRStageGameState::OnRep_WaveDefenseUIActive()
{
    OnWaveDefenseUIActiveChangedDelegate.Broadcast(bWaveDefenseUIActive);
}

void ADRStageGameState::OnRep_WaveTimerEndServerTime()
{
    // 클라이언트: 새 종료 시각 도착 시 로컬 1초 틱 시작
    StartLocalWaveTimerTick();
}

void ADRStageGameState::StartLocalWaveTimerTick()
{
    OnWaveTimerChangedDelegate.Broadcast(CurrentWaveNumber, GetWaveRemainingTime(), bIsWaveRestTime);
    GetWorldTimerManager().SetTimer(WaveTimerLocalTickHandle, this, &ADRStageGameState::BroadcastWaveTimerTick, 1.0f, true);
}

void ADRStageGameState::BroadcastWaveTimerTick()
{
    const float Remaining = GetWaveRemainingTime();
    OnWaveTimerChangedDelegate.Broadcast(CurrentWaveNumber, Remaining, bIsWaveRestTime);

    if (Remaining <= 0.0f)
    {
        GetWorldTimerManager().ClearTimer(WaveTimerLocalTickHandle);
    }
}

void ADRStageGameState::OnRep_IsWaveRestTime()
{
    // 클라이언트에서 Replicated 변수 변경 시 델리게이트 브로드캐스트
    OnWaveTimerChangedDelegate.Broadcast(CurrentWaveNumber, GetWaveRemainingTime(), bIsWaveRestTime);
}

void ADRStageGameState::OnRep_IsToxicGasWave()
{
    // 클라이언트에서 복제 후 브로드캐스트
    OnToxicGasWarningDelegate.Broadcast(bIsToxicGasWave);
}

// Actor의 World를 직접 사용해서 사운드 재생 (클라이언트에서 확실히 동작)
void ADRStageGameState::PlaySound2DFromSoundData(TObjectPtr<USoundBase> UDRSoundDataAsset::* SoundMember)
{
    if (UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized()))
    {
        if (UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset())
        {
            if (USoundBase* Sound = SoundData->*SoundMember)
            {
                UGameplayStatics::PlaySound2D(this, Sound);
            }
        }
    }
}

void ADRStageGameState::Multicast_PlayPhaseStartSound_Implementation()
{
    PlaySound2DFromSoundData(&UDRSoundDataAsset::PhaseStartSound);
}

void ADRStageGameState::Multicast_PlayWaveStartSound_Implementation()
{
    PlaySound2DFromSoundData(&UDRSoundDataAsset::WaveStartSound);
}

void ADRStageGameState::Multicast_PlayGameClearSound_Implementation()
{
    // BGM 정지
    TArray<AActor*> FoundBGMActors;
    UGameplayStatics::GetAllActorsOfClass(this, ADRBGMActor::StaticClass(), FoundBGMActors);
    for (AActor* BGMActor : FoundBGMActors)
    {
        if (ADRBGMActor* BGM = Cast<ADRBGMActor>(BGMActor))
        {
            BGM->StopBGM(2.0f);
        }
    }

    PlaySound2DFromSoundData(&UDRSoundDataAsset::GameClearSound);
}

void ADRStageGameState::Multicast_PlayGameOverSound_Implementation()
{
    // BGM 정지
    TArray<AActor*> FoundBGMActors;
    UGameplayStatics::GetAllActorsOfClass(this, ADRBGMActor::StaticClass(), FoundBGMActors);
    for (AActor* BGMActor : FoundBGMActors)
    {
        if (ADRBGMActor* BGM = Cast<ADRBGMActor>(BGMActor))
        {
            BGM->StopBGM(2.0f);
        }
    }

    PlaySound2DFromSoundData(&UDRSoundDataAsset::GameOverSound);
}

void ADRStageGameState::Multicast_PlayPoisonGasWarningSound_Implementation()
{
    PlaySound2DFromSoundData(&UDRSoundDataAsset::PoisonGasWarningSound);
}

void ADRStageGameState::UpdatePoisonGasLoopSound()
{
    if (bPoisonGasLoopActive)
    {
        // 이미 재생 중이면 무시 (멱등 처리)
        if (PoisonGasLoopAudioComponent && PoisonGasLoopAudioComponent->IsPlaying())
        {
            return;
        }

        if (UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized()))
        {
            if (UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset())
            {
                if (SoundData->PoisonGasActiveLoopSound)
                {
                    PoisonGasLoopAudioComponent = UGameplayStatics::SpawnSound2D(
                        this,
                        SoundData->PoisonGasActiveLoopSound,
                        1.0f, 1.0f, 0.0f,
                        nullptr, false, false
                    );
                }
            }
        }
    }
    else if (PoisonGasLoopAudioComponent)
    {
        PoisonGasLoopAudioComponent->Stop();
        PoisonGasLoopAudioComponent = nullptr;
    }
}

void ADRStageGameState::OnRep_PoisonGasLoopActive()
{
    UpdatePoisonGasLoopSound();
}

void ADRStageGameState::NotifyPoisonGasActivated()
{
    if (!HasAuthority()) return;

    ++ActivePoisonGasCount;
    if (ActivePoisonGasCount == 1)
    {
        bPoisonGasLoopActive = true;
        UpdatePoisonGasLoopSound();
    }
}

void ADRStageGameState::NotifyPoisonGasDeactivated()
{
    if (!HasAuthority()) return;

    --ActivePoisonGasCount;
    if (ActivePoisonGasCount <= 0)
    {
        ActivePoisonGasCount = 0;
        bPoisonGasLoopActive = false;
        UpdatePoisonGasLoopSound();
    }
}

// ========== Phase3 스폰 포인트 VFX ==========

void ADRStageGameState::SetEnemySpawnPointVFX(const TArray<FVector_NetQuantize>& Locations, UNiagaraSystem* NiagaraAsset)
{
    if (!HasAuthority()) return;

    EnemySpawnPointVFXLocations = Locations;
    EnemySpawnPointVFXAsset = NiagaraAsset;

    // 리슨 서버 호스트 로컬 반영 (클라이언트는 OnRep에서)
    RefreshEnemySpawnPointVFX();
}

void ADRStageGameState::ClearEnemySpawnPointVFX()
{
    SetEnemySpawnPointVFX(TArray<FVector_NetQuantize>(), nullptr);
}

void ADRStageGameState::SetEliteSpawnPointVFX(const TArray<FVector_NetQuantize>& Locations, UNiagaraSystem* NiagaraAsset)
{
    if (!HasAuthority()) return;

    EliteSpawnPointVFXLocations = Locations;
    EliteSpawnPointVFXAsset = NiagaraAsset;

    RefreshEliteSpawnPointVFX();
}

void ADRStageGameState::ClearEliteSpawnPointVFX()
{
    SetEliteSpawnPointVFX(TArray<FVector_NetQuantize>(), nullptr);
}

void ADRStageGameState::OnRep_EnemySpawnPointVFXLocations()
{
    RefreshEnemySpawnPointVFX();
}

void ADRStageGameState::OnRep_EliteSpawnPointVFXLocations()
{
    RefreshEliteSpawnPointVFX();
}

void ADRStageGameState::RefreshEnemySpawnPointVFX()
{
    for (UNiagaraComponent* Comp : EnemySpawnPointVFXComponents)
    {
        if (Comp && IsValid(Comp))
        {
            Comp->DeactivateImmediate();
            Comp->DestroyComponent();
        }
    }
    EnemySpawnPointVFXComponents.Empty();

    if (!EnemySpawnPointVFXAsset) return;

    for (const FVector_NetQuantize& Location : EnemySpawnPointVFXLocations)
    {
        UNiagaraComponent* NewComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            EnemySpawnPointVFXAsset,
            Location,
            FRotator::ZeroRotator,
            FVector(1.f),
            false,
            true,
            ENCPoolMethod::None,
            true
        );

        if (NewComp)
        {
            EnemySpawnPointVFXComponents.Add(NewComp);
        }
    }
}

void ADRStageGameState::RefreshEliteSpawnPointVFX()
{
    for (UNiagaraComponent* Comp : EliteSpawnPointVFXComponents)
    {
        if (Comp && IsValid(Comp))
        {
            Comp->DeactivateImmediate();
            Comp->DestroyComponent();
        }
    }
    EliteSpawnPointVFXComponents.Empty();

    if (!EliteSpawnPointVFXAsset) return;

    for (const FVector_NetQuantize& Location : EliteSpawnPointVFXLocations)
    {
        UNiagaraComponent* NewComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            EliteSpawnPointVFXAsset,
            Location,
            FRotator::ZeroRotator,
            FVector(1.f),
            false,
            true,
            ENCPoolMethod::None,
            true
        );

        if (NewComp)
        {
            EliteSpawnPointVFXComponents.Add(NewComp);
        }
    }
}

#undef LOCTEXT_NAMESPACE
