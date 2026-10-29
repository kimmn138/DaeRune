// Copyright DaeRune


#include "Phase/DRPhaseBase.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Actor/DRCleanserSite.h"
#include "Character/DREnemy.h"
#include "Engine/World.h"
#include "Interaction/CombatInterface.h"
#include "DaeRune/DRLogChannels.h"

// ========================= FDRShuffledIndexPool =========================

void FDRShuffledIndexPool::Reset(int32 InNum)
{
	Num = FMath::Max(0, InNum);
	Remaining.Reset(Num);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		Remaining.Add(Index);
	}
}

int32 FDRShuffledIndexPool::Pick()
{
	if (Num <= 0) return INDEX_NONE;

	if (Remaining.Num() == 0)
	{
		Reset(Num);
	}

	const int32 Slot = FMath::RandRange(0, Remaining.Num() - 1);
	const int32 Picked = Remaining[Slot];
	Remaining.RemoveAtSwap(Slot);
	return Picked;
}

// ========================= UDRPhaseBase =========================

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

	// 남은 적은 조용히 정리한다 (다음 페이즈로 넘어가지 않게)
	DestroyTrackedEnemies();
}

void UDRPhaseBase::SetCleanserSites(const TArray<ADRCleanserSite*>& InCleanserSites)
{
	CleanserSites.Reset();

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

ADREnemy* UDRPhaseBase::SpawnEnemyDeferred(TSubclassOf<ADREnemy> EnemyClass, const FTransform& SpawnTransform,
	TFunctionRef<void(ADREnemy&)> Configure)
{
	UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!World || !EnemyClass) return nullptr;

	ADREnemy* Enemy = World->SpawnActorDeferred<ADREnemy>(
		EnemyClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Enemy) return nullptr;

	// ★레벨 등은 BeginPlay 전에 넣어야 한다 — ADREnemy::BeginPlay 가 그 값으로 어트리뷰트와 어빌리티를 초기화한다
	Configure(*Enemy);
	Enemy->FinishSpawning(SpawnTransform);

	return IsValid(Enemy) ? Enemy : nullptr;
}

ADREnemy* UDRPhaseBase::SpawnEnemyDeferred(TSubclassOf<ADREnemy> EnemyClass, const FTransform& SpawnTransform)
{
	return SpawnEnemyDeferred(EnemyClass, SpawnTransform, [](ADREnemy&) {});
}

void UDRPhaseBase::TrackSpawnedEnemy(AActor* Enemy)
{
	if (!IsValid(Enemy)) return;

	if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Enemy))
	{
		CombatInterface->GetOnDeathDelegate().AddUniqueDynamic(this, &UDRPhaseBase::OnEnemyDeath);
	}
	Enemy->OnDestroyed.AddUniqueDynamic(this, &UDRPhaseBase::HandleTrackedEnemyDestroyed);

	SpawnedEnemies.AddUnique(Enemy);
}

void UDRPhaseBase::UnbindTrackedEnemy(AActor* Enemy)
{
	if (!Enemy) return;

	if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Enemy))
	{
		CombatInterface->GetOnDeathDelegate().RemoveDynamic(this, &UDRPhaseBase::OnEnemyDeath);
	}
	Enemy->OnDestroyed.RemoveDynamic(this, &UDRPhaseBase::HandleTrackedEnemyDestroyed);
}

void UDRPhaseBase::HandleTrackedEnemyDestroyed(AActor* DestroyedActor)
{
	// 정상 사망은 OnEnemyDeath 가 이미 목록에서 뺐다 — 목록에 남아 있다면 사망 신호 없이 사라진 경우다
	if (!bIsPhaseActive || !DestroyedActor || !SpawnedEnemies.Contains(DestroyedActor)) return;

	UE_LOG(LogDR, Warning, TEXT("[Phase] %s 가 사망 처리 없이 사라졌습니다(KillZ 낙하 등). 처치된 것으로 처리합니다."),
		*DestroyedActor->GetName());

	OnEnemyDeath(DestroyedActor);
}

void UDRPhaseBase::DestroyTrackedEnemies()
{
	// ★목록을 먼저 비운다★ — Destroy 중에 소멸 델리게이트가 불려도 "사라진 적"으로 오인하지 않게
	TArray<TWeakObjectPtr<AActor>> Enemies = MoveTemp(SpawnedEnemies);
	SpawnedEnemies.Reset();

	for (const TWeakObjectPtr<AActor>& EnemyPtr : Enemies)
	{
		if (AActor* Enemy = EnemyPtr.Get())
		{
			UnbindTrackedEnemy(Enemy);
			Enemy->Destroy();
		}
	}
}

int32 UDRPhaseBase::GetAliveEnemyCount() const
{
	// 파괴된 적의 약포인터는 세지 않는다
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& EnemyPtr : SpawnedEnemies)
	{
		if (EnemyPtr.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

void UDRPhaseBase::SetupPhaseObjective(int32 PhaseNumber)
{
	// "Phase%d" 행 조회는 행 이름 기반 조회에 위임 (Plan6 §5.2)
	SetupPhaseObjectiveByRow(FName(*FString::Printf(TEXT("Phase%d"), PhaseNumber)));
}

void UDRPhaseBase::SetupPhaseObjectiveByRow(FName RowName, int32 OverrideRequiredCount)
{
	if (!PhaseObjectiveDataTable || !GameState || RowName.IsNone()) return;

	const FPhaseObjectiveData* ObjectiveData =
		PhaseObjectiveDataTable->FindRow<FPhaseObjectiveData>(RowName, TEXT("SetupPhaseObjectiveByRow"));
	if (!ObjectiveData)
	{
		UE_LOG(LogDR, Error, TEXT("[Phase] 목표 DataTable 에 행이 없습니다: %s"), *RowName.ToString());
		return;
	}

	if (OverrideRequiredCount < 0)
	{
		GameState->SetPhaseObjective(*ObjectiveData);
		return;
	}

	// 값 복사 후 진행도 분모만 런타임 값으로 교체
	FPhaseObjectiveData Overridden = *ObjectiveData;
	Overridden.RequiredCount = OverrideRequiredCount;
	GameState->SetPhaseObjective(Overridden);
}
