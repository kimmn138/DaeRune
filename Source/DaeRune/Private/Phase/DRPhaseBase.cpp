// Copyright DaeRune


#include "Phase/DRPhaseBase.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Actor/DRCleanserSite.h"
#include "Interaction/CombatInterface.h"

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
}

void UDRPhaseBase::OnPhaseEnd()
{
	if (!bIsPhaseActive) return;

	bIsPhaseActive = false;

	// 델리게이트 언바인딩 및 남은 적들 정리
	for (TWeakObjectPtr<AActor> EnemyPtr : SpawnedEnemies)
	{
		if (EnemyPtr.IsValid())
		{
			// 델리게이트 언바인딩
			if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(EnemyPtr.Get()))
			{
				CombatInterface->GetOnDeathDelegate().RemoveDynamic(this, &UDRPhaseBase::OnEnemyDeath);
			}

			// 남은 적 제거
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
}

int32 UDRPhaseBase::GetAliveEnemyCount() const
{
	int32 AliveCount = 0;

	for (const TWeakObjectPtr<AActor>& EnemyPtr : SpawnedEnemies)
	{
		if (EnemyPtr.IsValid())
		{
			// 죽었는지 확인
			if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(EnemyPtr.Get()))
			{
				if (!CombatInterface->Execute_IsDead(EnemyPtr.Get()))
				{
					AliveCount++;
				}
			}
		}
	}

	return AliveCount;
}
