// Copyright DaeRune

#include "Phase/DRPhase3.h"

#include "DRGameplayTags.h"
#include "EngineUtils.h"
#include "Character/DREnemy.h"
#include "Actor/DRCleanserSite.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DRCleanserSiteAttributeSet.h"
#include "AbilitySystem/Data/GameBalanceConfig.h"
#include "Actor/DRPoisonGasActor.h"
#include "Actor/DRBGMActor.h"
#include "Kismet/GameplayStatics.h"

// Phase3 시작 시 공통 설정을 로드하고 첫 웨이브를 시작합니다.
void UDRPhase3::OnPhaseStart()
{
	Super::OnPhaseStart();

	if (!GameMode || !GameState) return;

	LoadPhase3ConfigFromBalanceConfig();

	// 페이즈 구조 개편: 기존 Phase3가 새 Phase2 역할로 표기됨
	SetupPhaseObjective(2);

	// 웨이브 타이머 + 클렌저 사이트 HP UI 표시 요청 (Plan6 §5.4)
	// 기존에는 위젯 컨트롤러가 "페이즈 인덱스 == 2"로 판단했으나, 이 UI를 쓰는 페이즈가 직접 켠다.
	GameState->SetWaveDefenseUIActive(true);

	GameState->SetCurrentWaveNumber(0);
	GameState->SetCurrentWaveLevel(0);
	GameState->SetTotalWaves(TotalWaveCount);
	BroadcastAliveEnemyObjective();

	InitializeCleanserSite();
	InitializeActiveSpawnPoints();
	FindEnemySpawnPoints();

	// 일반 적 스폰 포인트 VFX 활성화 (위치 배열 1회 복제)
	if (EnemySpawnPointNiagaraSystem)
	{
		TArray<FVector_NetQuantize> SpawnPointLocations;
		for (const TObjectPtr<AActor>& SpawnPoint : EnemySpawnPoints)
		{
			if (SpawnPoint)
			{
				SpawnPointLocations.Add(SpawnPoint->GetActorLocation());
			}
		}
		GameState->SetEnemySpawnPointVFX(SpawnPointLocations, EnemySpawnPointNiagaraSystem);
	}

	// 엘리트 보스 BGM 전환용 BGM 액터 캐시 (월드에 1개만 있다고 가정)
	CachedBGMActor = Cast<ADRBGMActor>(UGameplayStatics::GetActorOfClass(GameMode, ADRBGMActor::StaticClass()));

	StartNextWave();

	for (const TObjectPtr<ADRCleanserSite>& Site : ActiveCleanserSites)
	{
		if (Site)
		{
			Site->MulticastStartOperatingSound();
		}
	}
}

// Phase3 종료 시 타이머/델리게이트/스폰 상태를 정리합니다.
void UDRPhase3::OnPhaseEnd()
{
	Super::OnPhaseEnd();

	// 스폰 포인트 VFX 비활성화
	if (GameState)
	{
		GameState->ClearEnemySpawnPointVFX();
		GameState->ClearEliteSpawnPointVFX();

		// 웨이브 방어 UI 숨김 (Plan6 §5.4) - 게임오버로 페이즈가 끝나는 경로도 여기를 지난다
		GameState->SetWaveDefenseUIActive(false);
	}

	ClearAllPhaseTimers();

	for (const TObjectPtr<ADRCleanserSite>& Site : CleanserSites)
	{
		if (IsValid(Site))
		{
			if (UDRCleanserSiteAttributeSet* AttributeSet = Site->GetAttributeSet())
			{
				AttributeSet->OnHealthZeroDelegate.RemoveAll(this);
			}
			Site->MulticastStopOperatingSound();
		}
	}

	// 엘리트 보스 제거 (SkipToNextWave와 동일한 정리, 정규 종료 경로에도 필요)
	DestroyEliteBosses();

	ActiveSpawnPointIndices.Empty();
	ActiveBlueSpawnPointIndices.Empty();
	EnemySpawnPoints.Empty();
	SpawnPointPool.Reset(0);
	RemoveToxicGas();
}

// 객체 소멸 직전에 남은 타이머를 안전하게 해제합니다.
void UDRPhase3::BeginDestroy()
{
	ClearAllPhaseTimers();

	Super::BeginDestroy();
}

bool UDRPhase3::IsCompleted() const
{
	// 모든 웨이브 클리어 시 페이즈 완료
	return CurrentWaveNumber >= TotalWaveCount;
}

// 페이즈가 관리하는 모든 타이머를 해제합니다 (OnPhaseEnd/BeginDestroy 공용).
void UDRPhase3::ClearAllPhaseTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(WaveTimerHandle);
		TimerManager.ClearTimer(SpawnTimerHandle);
		TimerManager.ClearTimer(PoisonGasSpawnTimerHandle);
		TimerManager.ClearTimer(EliteSpawnVFXTimerHandle);
	}
}

// 엘리트 보스 사망(또는 사망 없이 소멸) 시 추적을 끝내고 BGM 을 되돌립니다.
void UDRPhase3::OnEliteEnemyDeath(AActor* DeadEnemy)
{
	// 사망 → (시체) 소멸 순서로 두 번 올 수 있다 — 이미 뺀 보스면 무시
	if (!DeadEnemy || !bIsPhaseActive || !EliteBosses.Contains(DeadEnemy)) return;

	EliteBosses.Remove(DeadEnemy);

	if (GetAliveEliteCount() == 0 && bEliteBossSpawned)
	{
		bEliteBossSpawned = false;
		EndEliteBossBGM();
	}

	BroadcastAliveEnemyObjective();
	CheckWaveCompletion();
}

// 일반 적 사망 시 웨이브 조기 종료 조건을 검사합니다.
void UDRPhase3::OnEnemyDeath(AActor* DeadEnemy)
{
	Super::OnEnemyDeath(DeadEnemy);

	BroadcastAliveEnemyObjective();
	CheckWaveCompletion();
}

int32 UDRPhase3::GetAliveEliteCount() const
{
	int32 AliveEliteCount = 0;
	for (const TWeakObjectPtr<AActor>& Elite : EliteBosses)
	{
		if (Elite.IsValid())
		{
			++AliveEliteCount;
		}
	}
	return AliveEliteCount;
}

// 현재 살아있는 일반 적 + 엘리트 보스 수를 페이즈 목표 UI에 전달합니다.
void UDRPhase3::BroadcastAliveEnemyObjective()
{
	if (!GameState) return;

	GameState->UpdatePhaseObjectiveProgress(GetAliveEnemyCount() + GetAliveEliteCount());
}

// 이번 웨이브의 모든 몬스터가 스폰 완료되고 모두 처치되면 즉시 웨이브를 종료합니다.
void UDRPhase3::CheckWaveCompletion()
{
	if (!bIsPhaseActive) return;
	if (CurrentWaveState != EWaveState::InProgress) return;

	// 예정된 스폰이 모두 완료되었는지 확인 (스폰 예정이 없는 웨이브는 타이머에 맡김)
	if (TotalSpawnTick <= 0) return;
	if (CurrentSpawnTick < TotalSpawnTick) return;
	if (CurrentSpawnCount < TotalSpawnCount) return;

	// 일반 적 · 엘리트 보스 생존자 확인
	if (GetAliveEnemyCount() > 0 || GetAliveEliteCount() > 0) return;

	EndCurrentWave();
}

// 다음 웨이브를 시작하고 스폰 타이머를 설정합니다.
void UDRPhase3::StartNextWave()
{
	CurrentWaveNumber++;

	if (CurrentWaveNumber > TotalWaveCount) return;

	if (CurrentWaveNumber > 1)
	{
		IncreaseWaveLevel(1);
	}

	CurrentWaveState = EWaveState::InProgress;
	CurrentSpawnCount = 0;
	TotalSpawnCount = 0;
	CurrentSpawnTick = 0;
	TotalSpawnTick = 0;
	CurrentSpawnCycleIndex = 0;
	SpawnPointPool.Reset(EnemySpawnPoints.Num());

	// 웨이브 시작 시 1회 계산해 캐시 - 이후 스폰 틱/휴식 전환은 캐시를 사용
	CachedWaveData = GetWaveData(CurrentWaveNumber);
	CachedWaveModifier = GetWaveLevelModifier(CurrentWaveLevel);
	const FWaveData& CurrentWave = CachedWaveData;
	const FWaveLevelModifier& Modifier = CachedWaveModifier;

	UWorld* World = GetWorld();
	if (!GameState || !World) return;

	GameState->SetCurrentWaveNumber(CurrentWaveNumber);
	GameState->SetCurrentWaveLevel(CurrentWaveLevel);
	GameState->SetIsWaveRestTime(false);
	// 종료 시각 1회 복제 - UI 카운트다운은 GameState가 각 머신에서 로컬 계산
	GameState->StartWaveTimer(CurrentWave.PlayDuration);
	GameState->Multicast_PlayWaveStartSound();

	// ★0 이면 스폰 수가 0 이 되고 웨이브 타이머까지 걸리지 않아 페이즈가 멈춘다 — 최소 1명으로 본다
	const int32 PlayerCount = FMath::Max(1, GameState->GetInitialPlayerCount());

	TotalSpawnCount = Modifier.SpawnCycleLengthPerPlayer * PlayerCount;
	TotalSpawnTick = FMath::DivideAndRoundUp(TotalSpawnCount, SpawnBatchSize);

	const bool bCanSpawnEnemies =
		EnemySpawnPoints.Num() >= SpawnBatchSize &&
		Modifier.MonsterSpawnCycle.Num() > 0 &&
		TotalSpawnTick > 0;

	if (bCanSpawnEnemies)
	{
		const float SpawnIntervalSeconds = FMath::Max(0.1f, Modifier.SpawnIntervalSeconds);
		World->GetTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&UDRPhase3::ProcessWaveSpawn,
			SpawnIntervalSeconds,
			true,
			0.0f
		);
	}

	World->GetTimerManager().SetTimer(
		WaveTimerHandle,
		this,
		&UDRPhase3::EndCurrentWave,
		CurrentWave.PlayDuration,
		false
	);

	// 독가스 웨이브 경고 UI (모든 클라이언트에 복제)
	GameState->SetIsToxicGasWave(Modifier.bSpawnToxicGas);

	if (Modifier.bSpawnToxicGas)
	{
		SpawnToxicGas();
	}

	if (!Modifier.bSpawnEliteBoss || !EliteBossClass || CleanserSites.Num() == 0 || !CleanserSites[0])
	{
		return;
	}

	// 보스 스폰 포인트 위치 수집
	TArray<FVector_NetQuantize> BossSpawnLocations;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(BossSpawnPointTag))
		{
			BossSpawnLocations.Add(It->GetActorLocation());
		}
	}

	if (BossSpawnLocations.Num() == 0) return;

	// 엘리트 스폰 포인트 VFX 활성화 (위치 배열 1회 복제)
	if (EliteSpawnPointNiagaraSystem)
	{
		GameState->SetEliteSpawnPointVFX(BossSpawnLocations, EliteSpawnPointNiagaraSystem);
	}

	for (const FVector_NetQuantize& BossSpawnLocation : BossSpawnLocations)
	{
		SpawnEliteMonster(BossSpawnLocation);
	}

	// 일정 시간 후 엘리트 VFX 비활성화
	World->GetTimerManager().SetTimer(
		EliteSpawnVFXTimerHandle,
		FTimerDelegate::CreateWeakLambda(GameState.Get(), [WeakGameState = TWeakObjectPtr<ADRStageGameState>(GameState)]()
		{
			if (ADRStageGameState* GS = WeakGameState.Get())
			{
				GS->ClearEliteSpawnPointVFX();
			}
		}),
		EliteSpawnVFXDuration,
		false
	);
}

// 현재 웨이브를 종료하고 휴식 시간으로 전환합니다.
void UDRPhase3::EndCurrentWave()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		World->GetTimerManager().ClearTimer(PoisonGasSpawnTimerHandle);
	}

	// 웨이브 종료 시 독가스 플래그 리셋 (연속 독가스 웨이브에서 RepNotify 재발동 보장)
	if (GameState)
	{
		GameState->SetIsToxicGasWave(false);
	}

	// 마지막 웨이브가 끝났다면 휴식 시간 없이 즉시 승패 판정
	if (CurrentWaveNumber >= TotalWaveCount)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(WaveTimerHandle);
		}

		CurrentWaveState = EWaveState::Completed;
		CheckVictoryConditions();
		return;
	}

	StartRestTime();
}

// 휴식 시간을 시작하고 UI용 남은 시간을 갱신합니다.
void UDRPhase3::StartRestTime()
{
	// 같은 웨이브 내 전환이므로 StartNextWave에서 계산한 캐시 사용
	const float RestDuration = CachedWaveData.RestDuration;

	if (GameState)
	{
		GameState->SetIsWaveRestTime(true);
		// 종료 시각 1회 복제 - UI 카운트다운은 GameState가 각 머신에서 로컬 계산
		GameState->StartWaveTimer(RestDuration);
	}

	CurrentWaveState = EWaveState::Rest;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(WaveTimerHandle, this, &UDRPhase3::EndRestTime, RestDuration, false);
	}
}

// 휴식 시간이 끝나면 다음 웨이브를 시작합니다.
void UDRPhase3::EndRestTime()
{
	StartNextWave();
}

// 치트: 현재 웨이브를 즉시 종료하고 다음 웨이브로 진입합니다. 마지막 웨이브였다면 페이즈를 종료합니다.
void UDRPhase3::SkipToNextWave()
{
	if (!bIsPhaseActive) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// 웨이브 관련 타이머 정리
	World->GetTimerManager().ClearTimer(WaveTimerHandle);
	World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	World->GetTimerManager().ClearTimer(PoisonGasSpawnTimerHandle);
	World->GetTimerManager().ClearTimer(EliteSpawnVFXTimerHandle);

	// 일반 적 · 엘리트 제거 (사망 처리로 세지 않으므로 CheckWaveCompletion 이 끼어들지 않는다)
	DestroyTrackedEnemies();
	DestroyEliteBosses();

	// 독가스 정리
	RemoveToxicGas();

	// 웨이브 카운터 리셋 및 UI 상태 정리
	CurrentSpawnCount = 0;
	TotalSpawnCount = 0;
	CurrentSpawnTick = 0;
	TotalSpawnTick = 0;

	if (GameState)
	{
		GameState->ClearEliteSpawnPointVFX();
		GameState->SetIsToxicGasWave(false);
	}

	// 마지막 웨이브였다면 페이즈 자체를 종료
	if (CurrentWaveNumber >= TotalWaveCount)
	{
		if (GameMode)
		{
			GameMode->ValidatePhaseCompletion();
		}
		return;
	}

	// 휴식 시간을 스킵하고 곧바로 다음 웨이브 시작
	CurrentWaveState = EWaveState::Waiting;
	StartNextWave();
}

// 스폰 주기마다 4개 위치 동시 스폰을 1회 처리합니다.
void UDRPhase3::ProcessWaveSpawn()
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (CurrentSpawnTick < TotalSpawnTick && TotalSpawnTick > 0 && CurrentSpawnCount < TotalSpawnCount)
	{
		SpawnMonsterByCycle();
		CurrentSpawnTick++;
	}

	if (CurrentSpawnTick >= TotalSpawnTick || CurrentSpawnCount >= TotalSpawnCount)
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}
}

// 맵에서 적 스폰 포인트 태그를 찾아 4개를 등록합니다.
void UDRPhase3::FindEnemySpawnPoints()
{
	EnemySpawnPoints.Reset();

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(EnemySpawnPointTag))
			{
				EnemySpawnPoints.Add(*It);
			}
		}
	}

	if (EnemySpawnPoints.Num() > SpawnBatchSize)
	{
		EnemySpawnPoints.SetNum(SpawnBatchSize);
	}

	SpawnPointPool.Reset(EnemySpawnPoints.Num());
}

// 현재 배치에서 아직 사용하지 않은 스폰 포인트를 무작위로 선택합니다.
AActor* UDRPhase3::SelectNextSpawnPoint()
{
	if (EnemySpawnPoints.Num() < SpawnBatchSize) return nullptr;

	const int32 SpawnPointIndex = SpawnPointPool.Pick();
	return EnemySpawnPoints.IsValidIndex(SpawnPointIndex) ? EnemySpawnPoints[SpawnPointIndex].Get() : nullptr;
}

// 사이클 값(EMonsterSpawnType)에 맞는 몬스터 클래스를 반환합니다.
TSubclassOf<ADREnemy> UDRPhase3::SelectMonsterClassByType(int32 MonsterType) const
{
	switch (static_cast<EMonsterSpawnType>(MonsterType))
	{
	case EMonsterSpawnType::Rush:
		if (RushMonsterClass) return RushMonsterClass;
		break;
	case EMonsterSpawnType::Stealth:
		if (StealthMonsterClass) return StealthMonsterClass;
		break;
	default:
		break;
	}

	return NormalMonsterClass;
}

// 현재 사이클 인덱스로 이번 스폰 타입(EMonsterSpawnType 값)을 계산합니다.
int32 UDRPhase3::GetCurrentCycleMonsterType(const FWaveLevelModifier& WaveModifier) const
{
	constexpr int32 DefaultType = static_cast<int32>(EMonsterSpawnType::Normal);

	if (WaveModifier.MonsterSpawnCycle.Num() == 0)
	{
		return DefaultType;
	}

	const int32 CycleIndex = CurrentSpawnCycleIndex % WaveModifier.MonsterSpawnCycle.Num();
	const int32 MonsterType = WaveModifier.MonsterSpawnCycle[CycleIndex];
	if (MonsterType < static_cast<int32>(EMonsterSpawnType::Normal) ||
		MonsterType > static_cast<int32>(EMonsterSpawnType::Stealth))
	{
		return DefaultType;
	}

	return MonsterType;
}

void UDRPhase3::ConfigureWaveEnemy(ADREnemy& Enemy) const
{
	Enemy.SetLevel(CurrentWaveLevel);
	Enemy.bIsPhase3Enemy = true;
	if (CurrentWaveLevel >= HighLevelEnrageWaveLevel)
	{
		Enemy.EnrageHealthThreshold = HighLevelEnrageThreshold;
	}
}

// 한 스폰 주기에서 4개 포인트에 몬스터를 동시에 스폰합니다.
void UDRPhase3::SpawnMonsterByCycle()
{
	if (!NormalMonsterClass || EnemySpawnPoints.Num() < SpawnBatchSize) return;

	// 배치마다 네 지점을 한 번씩 쓴다
	SpawnPointPool.Reset(EnemySpawnPoints.Num());

	for (int32 BatchSpawnIndex = 0; BatchSpawnIndex < SpawnBatchSize; ++BatchSpawnIndex)
	{
		if (CurrentSpawnCount >= TotalSpawnCount)
		{
			break;
		}

		const AActor* SpawnPointActor = SelectNextSpawnPoint();
		const TSubclassOf<ADREnemy> SelectedMonsterClass =
			SelectMonsterClassByType(GetCurrentCycleMonsterType(CachedWaveModifier));
		CurrentSpawnCycleIndex++;

		if (!SpawnPointActor || !SelectedMonsterClass) continue;

		// 위치 · 회전만 쓴다 (스폰 지점 액터의 스케일이 적에게 옮겨가지 않게)
		const FTransform SpawnTransform(SpawnPointActor->GetActorRotation(), SpawnPointActor->GetActorLocation());
		ADREnemy* SpawnedEnemy = SpawnEnemyDeferred(SelectedMonsterClass, SpawnTransform,
			[this](ADREnemy& Enemy) { ConfigureWaveEnemy(Enemy); });
		if (!SpawnedEnemy) continue;

		CurrentSpawnCount++;

		// 기존 글로벌 Buff.Elite 태그 시스템은 포효(Roar) 오라로 대체됨
		TrackSpawnedEnemy(SpawnedEnemy);
		BroadcastAliveEnemyObjective();
		CheckGameOverConditions();
	}
}

// 엘리트 몬스터를 스폰하고 사망 이벤트를 연결합니다.
void UDRPhase3::SpawnEliteMonster(const FVector& SpawnLocation)
{
	ADREnemy* SpawnedEnemy = SpawnEnemyDeferred(EliteBossClass, FTransform(SpawnLocation),
		[this](ADREnemy& Enemy) { ConfigureWaveEnemy(Enemy); });
	if (!SpawnedEnemy) return;

	EliteBosses.Add(SpawnedEnemy);
	BroadcastAliveEnemyObjective();

	if (!bEliteBossSpawned)
	{
		bEliteBossSpawned = true;
		StartEliteBossBGM();
	}

	// 사망과 (사망 없는) 소멸 모두 같은 처리 — 낙하로 사라진 보스가 웨이브를 붙잡지 않게
	if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(SpawnedEnemy))
	{
		CombatInterface->GetOnDeathDelegate().AddUniqueDynamic(this, &UDRPhase3::OnEliteEnemyDeath);
	}
	SpawnedEnemy->OnDestroyed.AddUniqueDynamic(this, &UDRPhase3::OnEliteEnemyDeath);
}

void UDRPhase3::DestroyEliteBosses()
{
	// 목록을 먼저 비운다 — Destroy 중 소멸 델리게이트가 와도 OnEliteEnemyDeath 가 무시하게
	TArray<TWeakObjectPtr<AActor>> Elites = MoveTemp(EliteBosses);
	EliteBosses.Reset();

	for (const TWeakObjectPtr<AActor>& ElitePtr : Elites)
	{
		AActor* Elite = ElitePtr.Get();
		if (!Elite) continue;

		if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Elite))
		{
			CombatInterface->GetOnDeathDelegate().RemoveDynamic(this, &UDRPhase3::OnEliteEnemyDeath);
		}
		Elite->OnDestroyed.RemoveDynamic(this, &UDRPhase3::OnEliteEnemyDeath);
		Elite->Destroy();
	}

	if (bEliteBossSpawned)
	{
		bEliteBossSpawned = false;
		EndEliteBossBGM();
	}
}

// 웨이브 레벨 기준 스폰 설정/특수 옵션을 조회합니다.
FWaveLevelModifier UDRPhase3::GetWaveLevelModifier(int32 WaveLevel) const
{
	const int32 ClampedLevel = FMath::Clamp(WaveLevel, 1, MaxWaveLevel);

	// 사이클 길이를 4의 배수 이상으로 맞추고, 사이클 배열을 그 길이로 채우거나 자른다
	auto NormalizeModifier = [](FWaveLevelModifier& Modifier)
	{
		if (Modifier.SpawnCycleLengthPerPlayer <= 0)
		{
			Modifier.SpawnCycleLengthPerPlayer = Modifier.MonsterSpawnCycle.Num();
		}
		Modifier.SpawnCycleLengthPerPlayer = FMath::Max(SpawnBatchSize, Modifier.SpawnCycleLengthPerPlayer);

		const int32 NormalType = static_cast<int32>(EMonsterSpawnType::Normal);
		Modifier.MonsterSpawnCycle.SetNum(Modifier.SpawnCycleLengthPerPlayer);
		for (int32& TypeValue : Modifier.MonsterSpawnCycle)
		{
			if (TypeValue < NormalType || TypeValue > static_cast<int32>(EMonsterSpawnType::Stealth))
			{
				TypeValue = NormalType;
			}
		}
	};

	FWaveLevelModifier Modifier;
	if (const FWaveLevelModifierRow* Row = WaveLevelModifierTable
		? WaveLevelModifierTable->FindRow<FWaveLevelModifierRow>(FName(*FString::Printf(TEXT("Level%d"), ClampedLevel)), TEXT(""))
		: nullptr)
	{
		Modifier.SpawnIntervalSeconds = Row->SpawnIntervalSeconds;
		Modifier.SpawnCycleLengthPerPlayer = Row->SpawnCycleLengthPerPlayer;
		Modifier.MonsterSpawnCycle = Row->MonsterSpawnCycle;
		Modifier.bSpawnToxicGas = Row->bSpawnToxicGas;
		Modifier.bSpawnEliteBoss = Row->bSpawnEliteBoss;
	}
	else if (const FWaveLevelModifier* Fallback = WaveLevelModifiers.Find(ClampedLevel))
	{
		Modifier = *Fallback;
	}
	else
	{
		Modifier = GetDefaultWaveLevelModifier(ClampedLevel);
	}

	NormalizeModifier(Modifier);
	return Modifier;
}

// 웨이브 레벨별 기본 스폰 설정을 제공합니다.
FWaveLevelModifier UDRPhase3::GetDefaultWaveLevelModifier(int32 WaveLevel) const
{
	FWaveLevelModifier Modifier;
	switch (FMath::Clamp(WaveLevel, 1, MaxWaveLevel))
	{
	case 1:
		Modifier.SpawnIntervalSeconds = 2.5f;
		Modifier.SpawnCycleLengthPerPlayer = 8;
		break;
	case 2:
	case 3:
		Modifier.SpawnIntervalSeconds = 2.0f;
		Modifier.SpawnCycleLengthPerPlayer = 8;
		break;
	case 4:
		Modifier.SpawnIntervalSeconds = 2.0f;
		Modifier.SpawnCycleLengthPerPlayer = 12;
		break;
	default:
		Modifier.SpawnIntervalSeconds = 1.5f;
		Modifier.SpawnCycleLengthPerPlayer = 16;
		break;
	}

	Modifier.MonsterSpawnCycle.Init(static_cast<int32>(EMonsterSpawnType::Normal), Modifier.SpawnCycleLengthPerPlayer);
	return Modifier;
}

// 웨이브 번호 기준 플레이/휴식 시간을 조회합니다. (DataTable → BP 폴백 배열 → 구조체 기본값)
FWaveData UDRPhase3::GetWaveData(int32 WaveNumber) const
{
	const int32 ClampedWave = FMath::Clamp(WaveNumber, 1, TotalWaveCount);

	if (WaveDataTable)
	{
		const FName RowName(*FString::Printf(TEXT("Wave%d"), ClampedWave));
		if (const FWaveDataRow* Row = WaveDataTable->FindRow<FWaveDataRow>(RowName, TEXT("")))
		{
			FWaveData WaveData;
			WaveData.PlayDuration = Row->PlayDuration;
			WaveData.RestDuration = Row->RestDuration;
			return WaveData;
		}
	}

	const int32 ArrayIndex = ClampedWave - 1;
	return WaveDataArray.IsValidIndex(ArrayIndex) ? WaveDataArray[ArrayIndex] : FWaveData();
}

// GameBalanceConfig에서 Phase3 공통 값을 로드합니다.
void UDRPhase3::LoadPhase3ConfigFromBalanceConfig()
{
	if (const UGameBalanceConfig* BalanceConfig = UDRAbilitySystemLibrary::GetGameBalanceConfig(GameMode))
	{
		MaxMonsterCount = BalanceConfig->Phase3.MaxMonsterCount;
		PoisonGasSpawnInterval = BalanceConfig->Phase3.PoisonGasSpawnInterval;
		HighLevelEnrageThreshold = BalanceConfig->Phase3.HighLevelEnrageThreshold;
	}
}

// 현재 웨이브 레벨을 증가시키고 범위를 제한합니다.
void UDRPhase3::IncreaseWaveLevel(int32 Amount)
{
	CurrentWaveLevel = FMath::Clamp(CurrentWaveLevel + Amount, 1, MaxWaveLevel);
}

// 활성 클렌저 사이트의 체력을 켜고 체력 0 → 게임오버를 연결합니다.
void UDRPhase3::InitializeCleanserSite()
{
	for (ADRCleanserSite* Site : ActiveCleanserSites)
	{
		if (!IsValid(Site)) continue;

		// ActorInfo는 ADRCleanserSite::BeginPlay에서 이미 초기화됨 - 여기서는 어트리뷰트만 적용
		if (Site->GetAbilitySystemComponent())
		{
			Site->InitializeDefaultAttributes();
		}

		if (UDRCleanserSiteAttributeSet* CleanserSiteAttributeSet = Site->GetAttributeSet())
		{
			CleanserSiteAttributeSet->OnHealthZeroDelegate.AddUniqueDynamic(this, &UDRPhase3::OnCleanserSiteHealthZero);
		}
	}
}

// 독가스 스폰 포인트 중 현재 활성 포인트 목록을 계산합니다.
void UDRPhase3::InitializeActiveSpawnPoints()
{
	ActiveSpawnPointIndices.Empty();
	ActiveBlueSpawnPointIndices.Empty();

	// 비활성 사이트에 연결된 지점은 FindPoisonGasSpawnPoints 가 이미 Normal 로 분류한다
	FindPoisonGasSpawnPoints();

	if (ActiveCleanserSites.Num() == 0) return;

	for (int32 i = 0; i < AllPoisonGasSpawnPoints.Num(); ++i)
	{
		ActiveSpawnPointIndices.Add(i);
		if (AllPoisonGasSpawnPoints[i].SpawnType == EPoisonGasSpawnPointType::CleanserLinked)
		{
			ActiveBlueSpawnPointIndices.Add(i);
		}
	}
}

// 레벨에 배치된 마커 액터를 태그로 탐색하여 독가스 스폰 포인트를 구성합니다.
void UDRPhase3::FindPoisonGasSpawnPoints()
{
	AllPoisonGasSpawnPoints.Empty();

	UWorld* World = GetWorld();
	if (!World) return;

	// 활성 클렌저 ID 수집
	TSet<FName> ActiveCleanserIDs;
	for (const TObjectPtr<ADRCleanserSite>& Site : ActiveCleanserSites)
	{
		if (Site && Site->GetCleanserID() != NAME_None)
		{
			ActiveCleanserIDs.Add(Site->GetCleanserID());
		}
	}

	const FString CleanserTagPrefixStr = PoisonGasCleanserTagPrefix.ToString();

	// 태그로 마커 액터 탐색
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor->ActorHasTag(PoisonGasSpawnPointTag)) continue;

		FPoisonGasSpawnPointData PointData;
		PointData.Location = Actor->GetActorLocation();

		// CleanserLinked 판별: "PoisonGas_CleanserA" 같은 태그가 있는지 확인
		for (const FName& Tag : Actor->Tags)
		{
			const FString TagStr = Tag.ToString();
			if (!TagStr.StartsWith(CleanserTagPrefixStr)) continue;

			const FName CleanserIDName(*TagStr.RightChop(CleanserTagPrefixStr.Len()));
			if (ActiveCleanserIDs.Contains(CleanserIDName))
			{
				PointData.SpawnType = EPoisonGasSpawnPointType::CleanserLinked;
				PointData.LinkedCleanserTag = CleanserIDName;
			}
			break;
		}

		AllPoisonGasSpawnPoints.Add(PointData);
	}
}

// 클렌저 체력이 0이 되면 게임오버를 처리합니다.
void UDRPhase3::OnCleanserSiteHealthZero()
{
	if (GameMode)
	{
		GameMode->TriggerGameOver();
	}
}

// 몬스터 수 제한 초과 여부를 검사합니다.
void UDRPhase3::CheckGameOverConditions() const
{
	if (IsMonsterCountExceeded() && GameMode)
	{
		GameMode->TriggerGameOver();
	}
}

// 마지막 웨이브 종료 시 승리/패배 조건을 검사합니다.
void UDRPhase3::CheckVictoryConditions()
{
	if (!GameMode) return;

	// 페이즈3에서 스폰된 적이 한 명이라도 살아있으면 게임 오버
	if (GetAliveEnemyCount() > 0 || GetAliveEliteCount() > 0)
	{
		GameMode->TriggerGameOver();
		return;
	}

	// 모든 적이 처치된 상태에서 클렌저 사이트가 살아있으면 클리어
	bool bAllSitesAlive = true;
	for (const TObjectPtr<ADRCleanserSite>& Site : ActiveCleanserSites)
	{
		if (!IsValid(Site))
		{
			bAllSitesAlive = false;
			break;
		}
	}

	if (bAllSitesAlive)
	{
		GameMode->ValidatePhaseCompletion();
	}
	else
	{
		GameMode->TriggerGameOver();
	}
}

// 생존 몬스터 수가 최대치를 넘었는지 확인합니다.
bool UDRPhase3::IsMonsterCountExceeded() const
{
	return GetAliveEnemyCount() >= MaxMonsterCount;
}

// 독가스 생성에 사용할 랜덤 포인트 인덱스를 고릅니다.
TArray<int32> UDRPhase3::SelectRandomSpawnPointIndices() const
{
	constexpr int32 MaxGasPerBatch = 8;

	if (ActiveSpawnPointIndices.Num() <= MaxGasPerBatch)
	{
		return ActiveSpawnPointIndices;
	}

	TArray<int32> SelectedIndices;

	// 클렌저에 연결된(파란) 지점 하나는 반드시 포함한다
	if (ActiveBlueSpawnPointIndices.Num() > 0)
	{
		SelectedIndices.Add(ActiveBlueSpawnPointIndices[FMath::RandRange(0, ActiveBlueSpawnPointIndices.Num() - 1)]);
	}

	TArray<int32> RemainingPoints = ActiveSpawnPointIndices;
	for (int32 SelectedIdx : SelectedIndices)
	{
		RemainingPoints.Remove(SelectedIdx);
	}

	while (SelectedIndices.Num() < MaxGasPerBatch && RemainingPoints.Num() > 0)
	{
		const int32 RandomIndex = FMath::RandRange(0, RemainingPoints.Num() - 1);
		SelectedIndices.Add(RemainingPoints[RandomIndex]);
		RemainingPoints.RemoveAtSwap(RandomIndex);
	}

	return SelectedIndices;
}

// 독가스 주기 스폰 타이머를 시작합니다.
void UDRPhase3::SpawnToxicGas()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PoisonGasSpawnTimerHandle,
			this,
			&UDRPhase3::SpawnPoisonGasActor,
			PoisonGasSpawnInterval,  // 반복 스폰 주기
			true,   // 반복 실행
			0.0f    // 첫 스폰 지연 시간 (경고 페이즈가 3초 지연을 대신함)
		);
	}
}

// 선택된 포인트에 독가스 액터를 스폰합니다.
void UDRPhase3::SpawnPoisonGasActor()
{
	UWorld* World = GetWorld();
	if (!World || !PoisonGasActorClass) return;

	// 수명(3.5초)이 끝나 사라진 가스는 목록에서 뺀다
	ToxicGasActors.RemoveAll([](const TWeakObjectPtr<AActor>& Gas) { return !Gas.IsValid(); });

	const TArray<int32> SelectedIndices = SelectRandomSpawnPointIndices();
	if (SelectedIndices.Num() == 0) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	bool bAnySpawned = false;
	for (int32 Index : SelectedIndices)
	{
		if (!AllPoisonGasSpawnPoints.IsValidIndex(Index)) continue;

		ADRPoisonGasActor* PoisonGas = World->SpawnActor<ADRPoisonGasActor>(
			PoisonGasActorClass, AllPoisonGasSpawnPoints[Index].Location, FRotator::ZeroRotator, SpawnParams);
		if (!PoisonGas) continue;

		PoisonGas->SetLifeSpan(3.5f);  // 경고 3초 + 활성 0.5초
		ToxicGasActors.Add(PoisonGas);
		bAnySpawned = true;
	}

	// 배치당 1회 경고 사운드
	if (bAnySpawned && GameState)
	{
		GameState->Multicast_PlayPoisonGasWarningSound();
	}
}

// 독가스 타이머와 남아있는 독가스 액터를 정리합니다.
void UDRPhase3::RemoveToxicGas()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PoisonGasSpawnTimerHandle);
	}

	for (const TWeakObjectPtr<AActor>& GasActor : ToxicGasActors)
	{
		if (AActor* Gas = GasActor.Get())
		{
			Gas->Destroy();
		}
	}

	ToxicGasActors.Empty();
}

// 엘리트 보스 활성 시 보스 BGM으로 전환합니다.
// (예전의 글로벌 Buff.Elite 태그 시스템은 포효(Roar) 스킬의 범위 기반 오라 버프로 대체되었다)
void UDRPhase3::StartEliteBossBGM()
{
	if (ADRBGMActor* BGMActor = CachedBGMActor.Get())
	{
		BGMActor->StartBossBGM();
	}
}

// 엘리트 보스 비활성 시 스테이지 BGM으로 복귀합니다.
void UDRPhase3::EndEliteBossBGM()
{
	if (ADRBGMActor* BGMActor = CachedBGMActor.Get())
	{
		BGMActor->EndBossBGM();
	}
}
