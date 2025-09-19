// Copyright DaeRune


#include "PlayLoop/StageManager.h"
#include "PlayLoop/DRPhaseBase.h"
#include "PlayLoop/DRPhase1.h"

AStageManager::AStageManager()
{

}

void AStageManager::BeginPlay()
{
    Super::BeginPlay();
    StartPhase1();
}

void AStageManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (CurrentPhase) CurrentPhase->Tick(DeltaSeconds);
}

void AStageManager::StartPhase1()
{
    if (CurrentPhase) { CurrentPhase->Exit(); CurrentPhase = nullptr; }

    UDRPhase1* P1 = CreatePhase<UDRPhase1>(Phase1Class);
    CurrentPhase = P1;

    if (CurrentPhase) CurrentPhase->Enter();
}

