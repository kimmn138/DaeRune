// Copyright DaeRune

#include "Phase/Stage2/DRS2PhaseBase.h"

#include "Actor/Stage2/DRS2StageDirector.h"
#include "Actor/Stage2/DRS2PassageBlocker.h"
#include "Actor/Stage2/DRS2TeleportGate.h"
#include "Actor/Stage2/DRS2EnemySpawnPoint.h"
#include "Character/DRCharacter.h"
#include "Character/DREnemy.h"
#include "EngineUtils.h"
#include "Game/DRGameStateBase.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Interaction/CombatInterface.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"
#include "DaeRune/DRLogChannels.h"

namespace
{
	// 스테이지2가 지원하는 최대 인원 구간
	constexpr int32 S2MaxPlayerCount = 4;
}

void UDRS2PhaseBase::OnPhaseEnd()
{
	// 자기 타이머를 먼저 정리한다. Super가 적을 파괴하는 동안 지연 스폰이 돌면 안 된다.
	ClearAllManagedTimers();
	PendingSpawnCount = 0;

	Super::OnPhaseEnd();
}

UWorld* UDRS2PhaseBase::GetPhaseWorld() const
{
	return GameMode ? GameMode->GetWorld() : nullptr;
}

ADRS2StageDirector* UDRS2PhaseBase::GetDirector()
{
	if (IsValid(CachedDirector)) return CachedDirector;

	UWorld* World = GetPhaseWorld();
	if (!World) return nullptr;

	for (TActorIterator<ADRS2StageDirector> It(World); It; ++It)
	{
		CachedDirector = *It;
		return CachedDirector;
	}

	if (!bDirectorLookupFailed)
	{
		bDirectorLookupFailed = true;
		UE_LOG(LogDR, Error, TEXT("[S2Phase] StageDirector 가 레벨에 없습니다. 스테이지2 진행 불가."));
	}

	return nullptr;
}

int32 UDRS2PhaseBase::ResolveBasePlayerCount()
{
	int32 AliveCount = 0;

	if (const ADRGameStateBase* DRGameState = Cast<ADRGameStateBase>(GameState))
	{
		AliveCount = DRGameState->GetAlivePlayers().Num();
	}

	BasePlayerCount = FMath::Clamp(AliveCount, 1, S2MaxPlayerCount);

	UE_LOG(LogDR, Log, TEXT("[S2Phase] 기준 인원 확정: %d (생존 %d)"), BasePlayerCount, AliveCount);

	return BasePlayerCount;
}

const FS2WaveSet* UDRS2PhaseBase::ResolveWaveSet(const TArray<FS2WaveSet>& WaveSets) const
{
	if (WaveSets.Num() == 0)
	{
		UE_LOG(LogDR, Error, TEXT("[S2Phase] WaveSetsByPlayerCount 가 비어 있습니다. 페이즈 BP 설정을 확인하세요."));
		return nullptr;
	}

	if (BasePlayerCount - 1 > WaveSets.Num() - 1)
	{
		UE_LOG(LogDR, Warning, TEXT("[S2Phase] 인원 %d 구간이 없어 마지막 구간(%d인)으로 폴백합니다."),
			BasePlayerCount, WaveSets.Num());
	}

	const int32 Index = FMath::Clamp(BasePlayerCount - 1, 0, WaveSets.Num() - 1);
	return &WaveSets[Index];
}

int32 UDRS2PhaseBase::CountComposition(const FS2WaveComposition& Composition)
{
	int32 Total = 0;
	for (const FS2EnemyCount& Entry : Composition.Enemies)
	{
		if (Entry.EnemyClass)
		{
			Total += FMath::Max(0, Entry.Count);
		}
	}
	return Total;
}

int32 UDRS2PhaseBase::CountTotalSpawns(const FS2WaveSet& WaveSet)
{
	int32 Total = 0;
	for (const FS2WaveComposition& Composition : WaveSet.Waves)
	{
		Total += CountComposition(Composition);
	}
	return Total;
}

void UDRS2PhaseBase::InitSpawnPoints(FName RoomID, const TArray<TObjectPtr<AActor>>& DirectorOverride)
{
	SpawnPoints.Reset();
	RemainingSpawnPointIndices.Reset();

	// Director에 명시 배선이 있으면 그것을 우선한다.
	if (DirectorOverride.Num() > 0)
	{
		for (const TObjectPtr<AActor>& Point : DirectorOverride)
		{
			if (IsValid(Point))
			{
				SpawnPoints.Add(Point);
			}
		}
	}
	else if (UWorld* World = GetPhaseWorld())
	{
		// 레벨에 배치된 스폰 지점을 RoomID로 수집한다 (스테이지1의 스폰 포인트 수집 방식 계승).
		for (TActorIterator<ADRS2EnemySpawnPoint> It(World); It; ++It)
		{
			ADRS2EnemySpawnPoint* Point = *It;
			if (IsValid(Point) && Point->GetRoomID() == RoomID)
			{
				SpawnPoints.Add(Point);
			}
		}
	}

	if (SpawnPoints.Num() == 0)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2Phase] %s 스폰 지점이 하나도 없습니다. 레벨에 ADRS2EnemySpawnPoint 를 배치하고 RoomID 를 확인하세요."),
			*RoomID.ToString());
		return;
	}

	UE_LOG(LogDR, Log, TEXT("[S2Phase] %s 스폰 지점 %d개 수집"), *RoomID.ToString(), SpawnPoints.Num());
}

AActor* UDRS2PhaseBase::SelectNextSpawnPoint()
{
	if (SpawnPoints.Num() == 0) return nullptr;

	// 풀이 비면 전체 인덱스로 다시 채운다 (모든 지점을 한 번씩 사용한 뒤 반복).
	if (RemainingSpawnPointIndices.Num() == 0)
	{
		for (int32 Index = 0; Index < SpawnPoints.Num(); ++Index)
		{
			RemainingSpawnPointIndices.Add(Index);
		}
	}

	const int32 PickedSlot = FMath::RandRange(0, RemainingSpawnPointIndices.Num() - 1);
	const int32 SpawnPointIndex = RemainingSpawnPointIndices[PickedSlot];
	RemainingSpawnPointIndices.RemoveAt(PickedSlot);

	return SpawnPoints.IsValidIndex(SpawnPointIndex) ? SpawnPoints[SpawnPointIndex].Get() : nullptr;
}

void UDRS2PhaseBase::ScheduleWaveSet(const FS2WaveSet& WaveSet)
{
	UWorld* World = GetPhaseWorld();
	if (!World) return;

	for (const FS2WaveComposition& Composition : WaveSet.Waves)
	{
		if (Composition.StartDelaySeconds <= 0.f)
		{
			SpawnComposition(Composition);
		}
		else
		{
			FTimerDelegate Delegate = FTimerDelegate::CreateUObject(
				this, &UDRS2PhaseBase::SpawnComposition, Composition);

			World->GetTimerManager().SetTimer(
				AddManagedTimer(), Delegate, Composition.StartDelaySeconds, false);
		}
	}
}

void UDRS2PhaseBase::SpawnComposition(FS2WaveComposition Composition)
{
	UWorld* World = GetPhaseWorld();
	if (!World) return;

	// 종류별 마리 수를 1마리 단위로 펼친 뒤 셔플한다.
	// 셔플하지 않으면 같은 종류가 몰려서 등장해 연출이 단조로워진다.
	TArray<TSubclassOf<ADREnemy>> FlatList;
	for (const FS2EnemyCount& Entry : Composition.Enemies)
	{
		if (!Entry.EnemyClass) continue;

		for (int32 i = 0; i < Entry.Count; ++i)
		{
			FlatList.Add(Entry.EnemyClass);
		}
	}

	for (int32 i = FlatList.Num() - 1; i > 0; --i)
	{
		FlatList.Swap(i, FMath::RandRange(0, i));
	}

	for (int32 i = 0; i < FlatList.Num(); ++i)
	{
		const float Delay = Composition.PerEnemySpawnInterval * i;

		if (Delay <= 0.f)
		{
			SpawnOneEnemy(FlatList[i]);
		}
		else
		{
			FTimerDelegate Delegate = FTimerDelegate::CreateUObject(
				this, &UDRS2PhaseBase::SpawnOneEnemy, FlatList[i]);

			World->GetTimerManager().SetTimer(AddManagedTimer(), Delegate, Delay, false);
		}
	}
}

void UDRS2PhaseBase::SpawnOneEnemy(TSubclassOf<ADREnemy> EnemyClass)
{
	// 스폰에 실패해도 PendingSpawnCount는 반드시 줄여야 한다.
	// 그러지 않으면 전멸 판정이 영원히 성립하지 않아 페이즈가 멈춘다.
	ON_SCOPE_EXIT
	{
		PendingSpawnCount = FMath::Max(0, PendingSpawnCount - 1);
	};

	if (!EnemyClass)
	{
		UE_LOG(LogDR, Warning, TEXT("[S2Phase] 스폰 실패 - 적 클래스가 비어 있습니다."));
		return;
	}

	const AActor* SpawnPoint = SelectNextSpawnPoint();
	if (!IsValid(SpawnPoint))
	{
		UE_LOG(LogDR, Warning, TEXT("[S2Phase] 스폰 실패 - 사용 가능한 스폰 지점이 없습니다."));
		return;
	}

	SpawnEnemyAt(EnemyClass, SpawnPoint->GetActorTransform());
}

AActor* UDRS2PhaseBase::SpawnEnemyAt(TSubclassOf<ADREnemy> EnemyClass, const FTransform& SpawnTransform, int32 EnemyLevel)
{
	UWorld* World = GetPhaseWorld();
	if (!World || !EnemyClass) return nullptr;

	AActor* SpawnedEnemy = nullptr;

	if (EnemyLevel > 0)
	{
		// ★레벨은 반드시 BeginPlay 전에 넣어야 한다 (헤더 주석 참고).
		//   ADREnemy::BeginPlay → InitAbilityActorInfo → InitializeDefaultAttributes(Level) 로
		//   커브 테이블(CT_EnemyAttributes)이 조회되고, 이어서 GiveStartupAbilities 가
		//   같은 Level 로 어빌리티 스펙을 부여한다. 스폰이 끝난 뒤 SetLevel 을 부르면 둘 다 놓친다.
		ADREnemy* DeferredEnemy = World->SpawnActorDeferred<ADREnemy>(
			EnemyClass, SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

		if (DeferredEnemy)
		{
			DeferredEnemy->SetLevel(EnemyLevel);
			DeferredEnemy->FinishSpawning(SpawnTransform);   // 여기서 BeginPlay 가 돈다
			SpawnedEnemy = DeferredEnemy;
		}
	}
	else
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		SpawnedEnemy = World->SpawnActor<AActor>(EnemyClass, SpawnTransform, SpawnParams);
	}

	if (!SpawnedEnemy) return nullptr;

	// 사망 델리게이트 바인딩 (DRPhase1::SpawnEnemy 관례)
	if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(SpawnedEnemy))
	{
		CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UDRPhaseBase::OnEnemyDeath);
	}

	SpawnedEnemies.Add(SpawnedEnemy);
	return SpawnedEnemy;
}

void UDRS2PhaseBase::SetBlockerBlocked(ADRS2PassageBlocker* Blocker, bool bBlocked)
{
	if (!IsValid(Blocker))
	{
		UE_LOG(LogDR, Error, TEXT("[S2Phase] 구조물 참조가 없습니다 (Director 배선 확인)."));
		return;
	}

	Blocker->SetBlocked(bBlocked);
}

void UDRS2PhaseBase::SetGateActive(ADRS2TeleportGate* Gate, bool bActive)
{
	if (!IsValid(Gate))
	{
		UE_LOG(LogDR, Error, TEXT("[S2Phase] 게이트 참조가 없습니다 (Director 배선 확인)."));
		return;
	}

	Gate->SetGateActive(bActive);
}

FTimerHandle& UDRS2PhaseBase::AddManagedTimer()
{
	return ManagedTimers.AddDefaulted_GetRef();
}

void UDRS2PhaseBase::ClearAllManagedTimers()
{
	if (UWorld* World = GetPhaseWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		for (FTimerHandle& Handle : ManagedTimers)
		{
			TimerManager.ClearTimer(Handle);
		}
	}

	ManagedTimers.Reset();
}
