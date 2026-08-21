// Copyright DaeRune


#include "Phase/DRPhaseBase.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Actor/DRCleanserSite.h"
#include "Interaction/CombatInterface.h"
#include "DaeRune/DRLogChannels.h"

void UDRPhaseBase::Initialize(ADRStageGameMode* InGameMode, ADRStageGameState* InGameState)
{
	if (!InGameMode || !InGameState) return;

	GameMode = InGameMode;
	GameState = InGameState;
}

void UDRPhaseBase::OnPhaseStart()
{
	if (!GameMode || !GameState) return;

	if (bIsPhaseActive) return;

	bIsPhaseActive = true;

	// Multicast RPC로 모든 클라이언트에서 사운드 재생
	GameState->Multicast_PlayPhaseStartSound();
}

void UDRPhaseBase::OnPhaseEnd()
{
	if (!bIsPhaseActive) return;

	bIsPhaseActive = false;

	// ��������Ʈ ����ε� �� ���� ���� ����
	for (TWeakObjectPtr<AActor> EnemyPtr : SpawnedEnemies)
	{
		if (EnemyPtr.IsValid())
		{
			// ��������Ʈ ����ε�
			if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(EnemyPtr.Get()))
			{
				CombatInterface->GetOnDeathDelegate().RemoveDynamic(this, &UDRPhaseBase::OnEnemyDeath);
			}

			// ���� �� ����
			EnemyPtr->Destroy();
		}
	}
	SpawnedEnemies.Empty();
}

void UDRPhaseBase::SetCleanserSites(const TArray<ADRCleanserSite*>& InCleanserSites)
{
	CleanserSites.Empty();

	for (ADRCleanserSite* Site : InCleanserSites)
	{
		if (Site)
		{
			CleanserSites.Add(Site);
		}
	}
}

void UDRPhaseBase::SetActiveCleanserSites(const TArray<TObjectPtr<ADRCleanserSite>>& InActiveSites)
{
	ActiveCleanserSites = InActiveSites;
}

void UDRPhaseBase::OnEnemyDeath(AActor* DeadEnemy)
{
	if (!DeadEnemy || !bIsPhaseActive) return;

	// SpawnedEnemies 배열에서 죽은 적 제거
	SpawnedEnemies.Remove(DeadEnemy);
}

int32 UDRPhaseBase::GetAliveEnemyCount() const
{
	return SpawnedEnemies.Num();
}

void UDRPhaseBase::SetupPhaseObjective(int32 PhaseNumber)
{
	// "Phase%d" 행 조회는 행 이름 기반 조회에 위임 (Plan6 §5.2)
	SetupPhaseObjectiveByRow(FName(*FString::Printf(TEXT("Phase%d"), PhaseNumber)));
}

void UDRPhaseBase::SetupPhaseObjectiveByRow(FName RowName)
{
	if (!PhaseObjectiveDataTable || !GameState || RowName.IsNone()) return;

	if (const FPhaseObjectiveData* ObjectiveData =
		PhaseObjectiveDataTable->FindRow<FPhaseObjectiveData>(RowName, TEXT("SetupPhaseObjectiveByRow")))
	{
		GameState->SetPhaseObjective(*ObjectiveData);
	}
	else
	{
		UE_LOG(LogDR, Error, TEXT("[Phase] 목표 DataTable 에 행이 없습니다: %s"), *RowName.ToString());
	}
}

void UDRPhaseBase::SetupPhaseObjectiveByRow(FName RowName, int32 OverrideRequiredCount)
{
	if (!PhaseObjectiveDataTable || !GameState || RowName.IsNone()) return;

	if (const FPhaseObjectiveData* ObjectiveData =
		PhaseObjectiveDataTable->FindRow<FPhaseObjectiveData>(RowName, TEXT("SetupPhaseObjectiveByRow")))
	{
		// 값 복사 후 진행도 분모만 런타임 값으로 교체
		FPhaseObjectiveData Overridden = *ObjectiveData;
		Overridden.RequiredCount = OverrideRequiredCount;
		GameState->SetPhaseObjective(Overridden);
	}
	else
	{
		UE_LOG(LogDR, Error, TEXT("[Phase] 목표 DataTable 에 행이 없습니다: %s"), *RowName.ToString());
	}
}
