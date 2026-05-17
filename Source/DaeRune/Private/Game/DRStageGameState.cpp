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
    bIsToxicGasWave = false;

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
    DOREPLIFETIME(ADRStageGameState, bIsToxicGasWave);

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

void ADRStageGameState::OnRep_IsToxicGasWave()
{
    // 클라이언트에서 복제 후 브로드캐스트
    OnToxicGasWarningDelegate.Broadcast(bIsToxicGasWave);
}

void ADRStageGameState::Multicast_PlayPhaseStartSound_Implementation()
{
    // Actor의 World를 직접 사용해서 사운드 재생 (클라이언트에서 확실히 동작)
    if (UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized()))
    {
        if (UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset())
        {
            if (SoundData->PhaseStartSound)
            {
                UGameplayStatics::PlaySound2D(this, SoundData->PhaseStartSound);
            }
        }
    }
}

void ADRStageGameState::Multicast_PlayWaveStartSound_Implementation()
{
    // Actor의 World를 직접 사용해서 사운드 재생 (클라이언트에서 확실히 동작)
    if (UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized()))
    {
        if (UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset())
        {
            if (SoundData->WaveStartSound)
            {
                UGameplayStatics::PlaySound2D(this, SoundData->WaveStartSound);
            }
        }
    }
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

    // Actor의 World를 직접 사용해서 사운드 재생 (클라이언트에서 확실히 동작)
    if (UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized()))
    {
        if (UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset())
        {
            if (SoundData->GameClearSound)
            {
                UGameplayStatics::PlaySound2D(this, SoundData->GameClearSound);
            }
        }
    }
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

    // Actor의 World를 직접 사용해서 사운드 재생 (클라이언트에서 확실히 동작)
    if (UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized()))
    {
        if (UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset())
        {
            if (SoundData->GameOverSound)
            {
                UGameplayStatics::PlaySound2D(this, SoundData->GameOverSound);
            }
        }
    }
}

void ADRStageGameState::Multicast_PlayPoisonGasWarningSound_Implementation()
{
    if (UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized()))
    {
        if (UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset())
        {
            if (SoundData->PoisonGasWarningSound)
            {
                UGameplayStatics::PlaySound2D(this, SoundData->PoisonGasWarningSound);
            }
        }
    }
}

void ADRStageGameState::Multicast_StartPoisonGasLoopSound_Implementation()
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

void ADRStageGameState::Multicast_StopPoisonGasLoopSound_Implementation()
{
    if (PoisonGasLoopAudioComponent)
    {
        PoisonGasLoopAudioComponent->Stop();
        PoisonGasLoopAudioComponent = nullptr;
    }
}

void ADRStageGameState::NotifyPoisonGasActivated()
{
    if (!HasAuthority()) return;

    ++ActivePoisonGasCount;
    if (ActivePoisonGasCount == 1)
    {
        Multicast_StartPoisonGasLoopSound();
    }
}

void ADRStageGameState::NotifyPoisonGasDeactivated()
{
    if (!HasAuthority()) return;

    --ActivePoisonGasCount;
    if (ActivePoisonGasCount <= 0)
    {
        ActivePoisonGasCount = 0;
        Multicast_StopPoisonGasLoopSound();
    }
}

// ========== Phase3 스폰 포인트 VFX ==========

void ADRStageGameState::Multicast_ActivateEnemySpawnPointVFX_Implementation(
    const TArray<FVector>& SpawnPointLocations, UNiagaraSystem* NiagaraAsset)
{
    if (!NiagaraAsset) return;

    for (UNiagaraComponent* Comp : EnemySpawnPointVFXComponents)
    {
        if (Comp && IsValid(Comp))
        {
            Comp->DeactivateImmediate();
            Comp->DestroyComponent();
        }
    }
    EnemySpawnPointVFXComponents.Empty();

    for (const FVector& Location : SpawnPointLocations)
    {
        UNiagaraComponent* NewComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            NiagaraAsset,
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

void ADRStageGameState::Multicast_DeactivateEnemySpawnPointVFX_Implementation()
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
}

void ADRStageGameState::Multicast_ActivateEliteSpawnPointVFX_Implementation(
    const TArray<FVector>& SpawnPointLocations, UNiagaraSystem* NiagaraAsset)
{
    if (!NiagaraAsset) return;

    for (UNiagaraComponent* Comp : EliteSpawnPointVFXComponents)
    {
        if (Comp && IsValid(Comp))
        {
            Comp->DeactivateImmediate();
            Comp->DestroyComponent();
        }
    }
    EliteSpawnPointVFXComponents.Empty();

    for (const FVector& Location : SpawnPointLocations)
    {
        UNiagaraComponent* NewComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            NiagaraAsset,
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

void ADRStageGameState::Multicast_DeactivateEliteSpawnPointVFX_Implementation()
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
}
