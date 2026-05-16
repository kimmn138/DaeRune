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
#include "Character/DRCharacter.h"

// Phase3 런타임 상태를 초기화합니다.
UDRPhase3::UDRPhase3()
{
	CurrentWaveNumber = 0;
	CurrentWaveLevel = 1; // 초기 웨이브 레벨은 1
	CurrentWaveState = EWaveState::Waiting;
	CurrentSpawnCount = 0;
	DefenseStartTime = 0.0f;
	WaveTimeRemaining = 0.0f;
	RestTimeRemaining = 0.0f;
	bEliteBossSpawned = false;
}

// Phase3 시작 시 공통 설정을 로드하고 첫 웨이브를 시작합니다.
void UDRPhase3::OnPhaseStart()
{
	Super::OnPhaseStart();

	if (!GameMode || !GameState) return;

	LoadPhase3ConfigFromBalanceConfig();

	SetupPhaseObjective(3);
	
	GameState->SetCurrentWaveNumber(0);
	GameState->SetCurrentWaveLevel(0);
	GameState->SetTotalWaves(5);
	
	InitializeCleanserSite();
	
	if (UWorld* World = GameMode->GetWorld())
	{
		InitializeActiveSpawnPoints();
		FindEnemySpawnPoints();

		// 일반 적 스폰 포인트 VFX 활성화
		if (GameState && EnemySpawnPointNiagaraSystem)
		{
			TArray<FVector> SpawnPointLocations;
			for (const TObjectPtr<AActor>& SpawnPoint : EnemySpawnPoints)
			{
				if (SpawnPoint)
				{
					SpawnPointLocations.Add(SpawnPoint->GetActorLocation());
				}
			}
			GameState->Multicast_ActivateEnemySpawnPointVFX(SpawnPointLocations, EnemySpawnPointNiagaraSystem);
		}

		DefenseStartTime = World->GetTimeSeconds();
		World->GetTimerManager().SetTimer(
			DefenseTimerHandle,
			this,
			&UDRPhase3::CheckVictoryConditions,
			DefenseDuration,
			false);
	}
	
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
		GameState->Multicast_DeactivateEnemySpawnPointVFX();
		GameState->Multicast_DeactivateEliteSpawnPointVFX();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaveTimerHandle);
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		World->GetTimerManager().ClearTimer(DefenseTimerHandle);
		World->GetTimerManager().ClearTimer(WaveTimerUpdateHandle);
		World->GetTimerManager().ClearTimer(PoisonGasSpawnTimerHandle);
		World->GetTimerManager().ClearTimer(EliteSpawnVFXTimerHandle);
	}

	for (const TObjectPtr<ADRCleanserSite>& Site : CleanserSites)
	{
		if (Site && IsValid(Site))
		{
			if (UDRCleanserSiteAttributeSet* AttributeSet = Site->GetAttributeSet())
			{
				AttributeSet->OnHealthBelowHalfDelegate.RemoveAll(this);
				AttributeSet->OnHealthZeroDelegate.RemoveAll(this);
			}
        
			Site->OnCleanserSiteDestroyed.RemoveAll(this);
			Site->MulticastStopOperatingSound();
		}
	}
	
	ActiveSpawnPointIndices.Empty();
	ActiveBlueSpawnPointIndices.Empty();
	EnemySpawnPoints.Empty();
	RemainingEnemySpawnPointIndices.Empty();
	RemoveToxicGas();
}

// 객체 소멸 직전에 남은 타이머를 안전하게 해제합니다.
void UDRPhase3::BeginDestroy()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaveTimerHandle);
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		World->GetTimerManager().ClearTimer(DefenseTimerHandle);
		World->GetTimerManager().ClearTimer(WaveTimerUpdateHandle);
		World->GetTimerManager().ClearTimer(PoisonGasSpawnTimerHandle);
		World->GetTimerManager().ClearTimer(EliteSpawnVFXTimerHandle);
	}

	Super::BeginDestroy();
}

// 엘리트 보스 사망 상태를 추적하고 관련 태그를 정리합니다.
void UDRPhase3::OnEliteEnemyDeath(AActor* DeadEnemy)
{
	if (!DeadEnemy || !bIsPhaseActive) return;

	EliteBosses.Remove(DeadEnemy);

	if (EliteBosses.IsEmpty())
	{
		bEliteBossSpawned = false;
		RemoveEliteBossTag();
	}

	CheckWaveCompletion();
}

// 일반 적 사망 시 웨이브 조기 종료 조건을 검사합니다.
void UDRPhase3::OnEnemyDeath(AActor* DeadEnemy)
{
	Super::OnEnemyDeath(DeadEnemy);

	CheckWaveCompletion();
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

	// 일반 적 생존자 확인
	if (GetAliveEnemyCount() > 0) return;

	// 엘리트 보스 생존자 확인
	for (const TWeakObjectPtr<AActor>& Elite : EliteBosses)
	{
		if (Elite.IsValid()) return;
	}

	EndCurrentWave();
}

// 다음 웨이브를 시작하고 스폰 타이머를 설정합니다.
void UDRPhase3::StartNextWave()
{
	CurrentWaveNumber++;

	if (CurrentWaveNumber > 5) return;

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
	RemainingEnemySpawnPointIndices.Empty();

	const FWaveData CurrentWave = GetWaveData(CurrentWaveNumber);
	const FWaveLevelModifier Modifier = GetWaveLevelModifier(CurrentWaveLevel);
	WaveTimeRemaining = CurrentWave.PlayDuration;

	if (GameState)
	{
		GameState->SetCurrentWaveNumber(CurrentWaveNumber);
		GameState->SetCurrentWaveLevel(CurrentWaveLevel);
		GameState->SetIsWaveRestTime(false);
		GameState->SetWaveRemainingTime(WaveTimeRemaining);
	}

	if (GameMode && GameState)
	{
		if (const UWorld* World = GameMode->GetWorld())
		{
			const int32 InitialPlayerCount = GameState->GetInitialPlayerCount();
			if (InitialPlayerCount <= 0) return;

			GameState->Multicast_PlayWaveStartSound();

			TotalSpawnCount = Modifier.SpawnCycleLengthPerPlayer * InitialPlayerCount;
			TotalSpawnTick = (TotalSpawnCount + 3) / 4;

			const bool bCanSpawnEnemies =
				EnemySpawnPoints.Num() >= 4 &&
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

			TWeakObjectPtr<UDRPhase3> WeakThis(this);
			World->GetTimerManager().SetTimer(
				WaveTimerUpdateHandle,
				[WeakThis]()
				{
					if (UDRPhase3* StrongThis = WeakThis.Get())
					{
						StrongThis->WaveTimeRemaining = FMath::Max(0.0f, StrongThis->WaveTimeRemaining - 1.0f);
						if (StrongThis->GameState)
						{
							StrongThis->GameState->SetWaveRemainingTime(StrongThis->WaveTimeRemaining);
						}
					}
				},
				1.0f,
				true,
				0.0f
			);
		}
	}

	// 독가스 웨이브 경고 UI (모든 클라이언트에 복제)
	if (GameState)
	{
		GameState->SetIsToxicGasWave(Modifier.bSpawnToxicGas);
	}

	if (Modifier.bSpawnToxicGas)
	{
		SpawnToxicGas();
	}

	if (Modifier.bSpawnEliteBoss && EliteBossClass)
	{
		if (CleanserSites.Num() > 0 && CleanserSites[0])
		{
			// 보스 스폰 포인트 위치 수집
			TArray<FVector> BossSpawnLocations;
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				AActor* BossSpawnPoint = *It;
				if (BossSpawnPoint && BossSpawnPoint->ActorHasTag(BossSpawnPointTag))
				{
					BossSpawnLocations.Add(BossSpawnPoint->GetActorLocation());
				}
			}

			// 엘리트 스폰 포인트 VFX 활성화
			if (GameState && EliteSpawnPointNiagaraSystem && BossSpawnLocations.Num() > 0)
			{
				GameState->Multicast_ActivateEliteSpawnPointVFX(BossSpawnLocations, EliteSpawnPointNiagaraSystem);
			}

			// 엘리트 보스 스폰
			for (const FVector& Location : BossSpawnLocations)
			{
				SpawnEliteMonster(Location);
			}

			// 일정 시간 후 엘리트 VFX 비활성화
			if (GameState && BossSpawnLocations.Num() > 0)
			{
				if (UWorld* SpawnWorld = GetWorld())
				{
					SpawnWorld->GetTimerManager().ClearTimer(EliteSpawnVFXTimerHandle);

					TWeakObjectPtr<ADRStageGameState> WeakGameState(GameState);
					SpawnWorld->GetTimerManager().SetTimer(
						EliteSpawnVFXTimerHandle,
						[WeakGameState]()
						{
							if (ADRStageGameState* GS = WeakGameState.Get())
							{
								GS->Multicast_DeactivateEliteSpawnPointVFX();
							}
						},
						EliteSpawnVFXDuration,
						false
					);
				}
			}
		}
	}
}
// 현재 웨이브를 종료하고 휴식 시간으로 전환합니다.
void UDRPhase3::EndCurrentWave()
{
	if (!GameMode) return;
	
	if (UWorld* World = GameMode->GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		World->GetTimerManager().ClearTimer(PoisonGasSpawnTimerHandle);
	}

	// 웨이브 종료 시 독가스 플래그 리셋 (연속 독가스 웨이브에서 RepNotify 재발동 보장)
	if (GameState)
	{
		GameState->SetIsToxicGasWave(false);
		GameState->UpdatePhaseObjectiveProgress(CurrentWaveNumber);
	}

	StartRestTime();
}

// 휴식 시간을 시작하고 UI용 남은 시간을 갱신합니다.
void UDRPhase3::StartRestTime()
{
	if (UWorld* World = GameMode->GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaveTimerUpdateHandle);
	}

	const FWaveData CurrentWave = GetWaveData(CurrentWaveNumber);
	RestTimeRemaining = CurrentWave.RestDuration;

	if (GameState)
	{
		GameState->SetIsWaveRestTime(true);
		GameState->SetWaveRemainingTime(RestTimeRemaining);
	}
	
	CurrentWaveState = EWaveState::Rest;
	
	if (GameMode)
	{
		if (const UWorld* World = GameMode->GetWorld())
		{
			TWeakObjectPtr<UDRPhase3> WeakThis(this);
			World->GetTimerManager().SetTimer(
				WaveTimerUpdateHandle,
				[WeakThis]()
				{
					if (UDRPhase3* StrongThis = WeakThis.Get())
					{
						StrongThis->RestTimeRemaining = FMath::Max(0.0f, StrongThis->RestTimeRemaining - 1.0f);
						if (StrongThis->GameState)
						{
							StrongThis->GameState->SetWaveRemainingTime(StrongThis->RestTimeRemaining);
						}
					}
				},
				1.0f,
				true,
				0.0f
			);

			World->GetTimerManager().SetTimer(
				WaveTimerHandle,
				this,
				&UDRPhase3::EndRestTime,
				CurrentWave.RestDuration,
				false
			);
		}
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
	World->GetTimerManager().ClearTimer(WaveTimerUpdateHandle);
	World->GetTimerManager().ClearTimer(PoisonGasSpawnTimerHandle);
	World->GetTimerManager().ClearTimer(EliteSpawnVFXTimerHandle);

	// 일반 적 제거 (사망 델리게이트는 분리하여 CheckWaveCompletion이 호출되지 않도록 함)
	for (TWeakObjectPtr<AActor>& EnemyPtr : SpawnedEnemies)
	{
		if (EnemyPtr.IsValid())
		{
			if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(EnemyPtr.Get()))
			{
				CombatInterface->GetOnDeathDelegate().RemoveDynamic(this, &UDRPhaseBase::OnEnemyDeath);
			}
			EnemyPtr->Destroy();
		}
	}
	SpawnedEnemies.Empty();

	// 엘리트 보스 제거
	for (TWeakObjectPtr<AActor>& ElitePtr : EliteBosses)
	{
		if (ElitePtr.IsValid())
		{
			if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(ElitePtr.Get()))
			{
				CombatInterface->GetOnDeathDelegate().RemoveDynamic(this, &UDRPhase3::OnEliteEnemyDeath);
			}
			ElitePtr->Destroy();
		}
	}
	EliteBosses.Empty();

	if (bEliteBossSpawned)
	{
		bEliteBossSpawned = false;
		RemoveEliteBossTag();
	}

	// 독가스 정리
	RemoveToxicGas();

	// 웨이브 카운터 리셋 및 UI 상태 정리
	CurrentSpawnCount = 0;
	TotalSpawnCount = 0;
	CurrentSpawnTick = 0;
	TotalSpawnTick = 0;

	if (GameState)
	{
		GameState->Multicast_DeactivateEliteSpawnPointVFX();
		GameState->SetIsToxicGasWave(false);
		GameState->UpdatePhaseObjectiveProgress(CurrentWaveNumber);
	}

	// 마지막 웨이브였다면 페이즈 자체를 종료
	if (CurrentWaveNumber >= 5)
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
	if (!GameMode) return;

	const UWorld* World = GameMode->GetWorld();
	if (!World) return;

	if (CurrentSpawnTick >= TotalSpawnTick || TotalSpawnTick <= 0 || CurrentSpawnCount >= TotalSpawnCount)
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		return;
	}

	SpawnMonsterByCycle();
	CurrentSpawnTick++;

	if (CurrentSpawnTick >= TotalSpawnTick || CurrentSpawnCount >= TotalSpawnCount)
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}
}


// 맵에서 적 스폰 포인트 태그를 찾아 4개를 등록합니다.
void UDRPhase3::FindEnemySpawnPoints()
{
	if (!GameMode) return;

	UWorld* World = GameMode->GetWorld();
	if (!World) return;

	EnemySpawnPoints.Empty();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->ActorHasTag(EnemySpawnPointTag))
		{
			EnemySpawnPoints.Add(Actor);
		}
	}

	if (EnemySpawnPoints.Num() < 4)
	{
}
	else if (EnemySpawnPoints.Num() > 4)
	{
EnemySpawnPoints.SetNum(4);
	}

	RemainingEnemySpawnPointIndices.Empty();
	for (int32 Index = 0; Index < EnemySpawnPoints.Num(); ++Index)
	{
		RemainingEnemySpawnPointIndices.Add(Index);
	}
}

// 현재 배치에서 아직 사용하지 않은 스폰 포인트를 무작위로 선택합니다.
AActor* UDRPhase3::SelectNextSpawnPoint()
{
	if (EnemySpawnPoints.Num() < 4)
	{
		return nullptr;
	}

	if (RemainingEnemySpawnPointIndices.Num() == 0)
	{
		for (int32 Index = 0; Index < EnemySpawnPoints.Num(); ++Index)
		{
			RemainingEnemySpawnPointIndices.Add(Index);
		}
	}

	if (RemainingEnemySpawnPointIndices.Num() == 0)
	{
		return nullptr;
	}

	const int32 RandomArrayIndex = FMath::RandRange(0, RemainingEnemySpawnPointIndices.Num() - 1);
	const int32 SpawnPointIndex = RemainingEnemySpawnPointIndices[RandomArrayIndex];
	RemainingEnemySpawnPointIndices.RemoveAt(RandomArrayIndex);

	if (!EnemySpawnPoints.IsValidIndex(SpawnPointIndex))
	{
		return nullptr;
	}

	return EnemySpawnPoints[SpawnPointIndex].Get();
}

// 사이클 숫자(1/2/3)에 맞는 몬스터 클래스를 반환합니다.
TSubclassOf<ADREnemy> UDRPhase3::SelectMonsterClassByType(int32 MonsterType) const
{
	switch (MonsterType)
	{
	case 2:
		if (RushMonsterClass) return RushMonsterClass;
		break;
	case 3:
		if (StealthMonsterClass) return StealthMonsterClass;
		break;
	default:
		break;
	}

	return NormalMonsterClass;
}

// 현재 사이클 인덱스로 이번 스폰 타입(1/2/3)을 계산합니다.
int32 UDRPhase3::GetCurrentCycleMonsterType(const FWaveLevelModifier& WaveModifier) const
{
	if (WaveModifier.MonsterSpawnCycle.Num() == 0)
	{
		return 1;
	}

	const int32 CycleIndex = CurrentSpawnCycleIndex % WaveModifier.MonsterSpawnCycle.Num();
	const int32 MonsterType = WaveModifier.MonsterSpawnCycle[CycleIndex];
	if (MonsterType < 1 || MonsterType > 3)
	{
		return 1;
	}

	return MonsterType;
}

// 한 스폰 주기에서 4개 포인트에 몬스터를 동시에 스폰합니다.
void UDRPhase3::SpawnMonsterByCycle()
{
	if (!GameMode) return;

	UWorld* World = GameMode->GetWorld();
	if (!World) return;

	if (!NormalMonsterClass || EnemySpawnPoints.Num() < 4) return;

	const FWaveLevelModifier WaveModifier = GetWaveLevelModifier(CurrentWaveLevel);

	RemainingEnemySpawnPointIndices.Empty();
	for (int32 Index = 0; Index < EnemySpawnPoints.Num(); ++Index)
	{
		RemainingEnemySpawnPointIndices.Add(Index);
	}

	for (int32 BatchSpawnIndex = 0; BatchSpawnIndex < 4; ++BatchSpawnIndex)
	{
		if (CurrentSpawnCount >= TotalSpawnCount)
		{
			break;
		}

		AActor* SpawnPointActor = SelectNextSpawnPoint();
		const int32 MonsterType = GetCurrentCycleMonsterType(WaveModifier);
		TSubclassOf<ADREnemy> SelectedMonsterClass = SelectMonsterClassByType(MonsterType);
		if (!SelectedMonsterClass)
		{
			SelectedMonsterClass = NormalMonsterClass;
		}

		if (SpawnPointActor && SelectedMonsterClass)
		{
			const FVector SpawnLocation = SpawnPointActor->GetActorLocation();
			const FRotator SpawnRotation = SpawnPointActor->GetActorRotation();

			ADREnemy* SpawnedEnemy = World->SpawnActorDeferred<ADREnemy>(
				SelectedMonsterClass,
				FTransform(SpawnRotation, SpawnLocation),
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
			);

			if (SpawnedEnemy)
			{
				SpawnedEnemy->SetLevel(CurrentWaveLevel);
				SpawnedEnemy->bIsPhase3Enemy = true;
				if (CurrentWaveLevel >= 4)
				{
					SpawnedEnemy->EnrageHealthThreshold = HighLevelEnrageThreshold;
				}

				SpawnedEnemy->FinishSpawning(FTransform(SpawnRotation, SpawnLocation));

				UE_LOG(LogTemp, Warning, TEXT("[DRPhase3] Spawned %s from point [%s] | RequestedLoc=%s | FinalLoc=%s | Controller=%s"),
					*SpawnedEnemy->GetName(),
					*SpawnPointActor->GetName(),
					*SpawnLocation.ToString(),
					*SpawnedEnemy->GetActorLocation().ToString(),
					SpawnedEnemy->GetController() ? *SpawnedEnemy->GetController()->GetName() : TEXT("NULL"));

				if (SpawnedEnemy->IsValidLowLevel())
				{
					CurrentSpawnCount++;

					if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(SpawnedEnemy))
					{
						CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UDRPhaseBase::OnEnemyDeath);
					}

					// 기존 글로벌 Buff.Elite 태그 시스템은 포효(Roar) 오라로 대체됨

					SpawnedEnemies.Add(SpawnedEnemy);
					CheckGameOverConditions();
				}
			}
		}

		CurrentSpawnCycleIndex++;
	}
}
// 엘리트 몬스터를 스폰하고 사망 이벤트를 연결합니다.
void UDRPhase3::SpawnEliteMonster(const FVector& SpawnLocation)
{
	if (!GameMode) return;

	TSubclassOf<ADREnemy> MonsterClass = EliteBossClass;
	
	UWorld* World = GameMode->GetWorld();
	if (!World) return;
	
	ADREnemy* SpawnedEnemy = World->SpawnActorDeferred<ADREnemy>(
		MonsterClass,
		FTransform(FRotator::ZeroRotator, SpawnLocation),
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
	);
	
	if (SpawnedEnemy)
	{
		SpawnedEnemy->SetLevel(CurrentWaveLevel);

		SpawnedEnemy->bIsPhase3Enemy = true;
		if (CurrentWaveLevel >= 4)
		{
			SpawnedEnemy->EnrageHealthThreshold = HighLevelEnrageThreshold;
		}
		
		SpawnedEnemy->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));
		
		EliteBosses.Add(SpawnedEnemy);

		if (!bEliteBossSpawned)
		{
			bEliteBossSpawned = true;
			GrantEliteBossTag();
		}

		if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(SpawnedEnemy))
		{
			CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UDRPhase3::OnEliteEnemyDeath);
		}
	}
}

// 웨이브 레벨 기준 스폰 설정/특수 옵션을 조회합니다.
FWaveLevelModifier UDRPhase3::GetWaveLevelModifier(int32 WaveLevel) const
{
	const int32 ClampedLevel = FMath::Clamp(WaveLevel, 1, 5);
	auto NormalizeModifier = [ClampedLevel](FWaveLevelModifier& Modifier)
	{
		if (Modifier.SpawnCycleLengthPerPlayer <= 0)
		{
			Modifier.SpawnCycleLengthPerPlayer = Modifier.MonsterSpawnCycle.Num();
		}

		Modifier.SpawnCycleLengthPerPlayer = FMath::Max(4, Modifier.SpawnCycleLengthPerPlayer);
		if (Modifier.SpawnCycleLengthPerPlayer % 4 != 0)
		{
}

		if (Modifier.MonsterSpawnCycle.Num() < Modifier.SpawnCycleLengthPerPlayer)
		{
			const int32 FillCount = Modifier.SpawnCycleLengthPerPlayer - Modifier.MonsterSpawnCycle.Num();
			for (int32 i = 0; i < FillCount; ++i)
			{
				Modifier.MonsterSpawnCycle.Add(1);
			}
		}
		else if (Modifier.MonsterSpawnCycle.Num() > Modifier.SpawnCycleLengthPerPlayer)
		{
			Modifier.MonsterSpawnCycle.SetNum(Modifier.SpawnCycleLengthPerPlayer);
		}

		for (int32& TypeValue : Modifier.MonsterSpawnCycle)
		{
			if (TypeValue < 1 || TypeValue > 3)
			{
				TypeValue = 1;
			}
		}
	};

	if (WaveLevelModifierTable)
	{
		const FString RowName = FString::Printf(TEXT("Level%d"), ClampedLevel);
		if (const FWaveLevelModifierRow* Row = WaveLevelModifierTable->FindRow<FWaveLevelModifierRow>(FName(*RowName), TEXT("")))
		{
			FWaveLevelModifier Modifier;
			Modifier.SpawnIntervalSeconds = Row->SpawnIntervalSeconds;
			Modifier.SpawnCycleLengthPerPlayer = Row->SpawnCycleLengthPerPlayer;
			Modifier.MonsterSpawnCycle = Row->MonsterSpawnCycle;
			Modifier.bSpawnToxicGas = Row->bSpawnToxicGas;
			Modifier.bSpawnEliteBoss = Row->bSpawnEliteBoss;
			NormalizeModifier(Modifier);
			return Modifier;
		}
	}

	if (const FWaveLevelModifier* Modifier = WaveLevelModifiers.Find(ClampedLevel))
	{
		FWaveLevelModifier Result = *Modifier;
		NormalizeModifier(Result);
		return Result;
	}

	FWaveLevelModifier DefaultModifier = GetDefaultWaveLevelModifier(ClampedLevel);
	NormalizeModifier(DefaultModifier);
	return DefaultModifier;
}

// 웨이브 레벨별 기본 스폰 설정을 제공합니다.
FWaveLevelModifier UDRPhase3::GetDefaultWaveLevelModifier(int32 WaveLevel) const
{
	const int32 ClampedLevel = FMath::Clamp(WaveLevel, 1, 5);

	FWaveLevelModifier Modifier;
	switch (ClampedLevel)
	{
	case 1:
		Modifier.SpawnIntervalSeconds = 2.5f;
		Modifier.SpawnCycleLengthPerPlayer = 8;
		break;
	case 2:
		Modifier.SpawnIntervalSeconds = 2.0f;
		Modifier.SpawnCycleLengthPerPlayer = 8;
		break;
	case 3:
		Modifier.SpawnIntervalSeconds = 2.0f;
		Modifier.SpawnCycleLengthPerPlayer = 8;
		break;
	case 4:
		Modifier.SpawnIntervalSeconds = 2.0f;
		Modifier.SpawnCycleLengthPerPlayer = 12;
		break;
	case 5:
		Modifier.SpawnIntervalSeconds = 1.5f;
		Modifier.SpawnCycleLengthPerPlayer = 16;
		break;
	default:
		break;
	}

	Modifier.MonsterSpawnCycle.Init(1, Modifier.SpawnCycleLengthPerPlayer);
	return Modifier;
}

// 웨이브 번호 기준 플레이/휴식 시간을 조회합니다.
FWaveData UDRPhase3::GetWaveData(int32 WaveNumber) const
{
	const int32 ClampedWave = FMath::Clamp(WaveNumber, 1, 5);

	if (WaveDataTable)
	{
		const FString RowName = FString::Printf(TEXT("Wave%d"), ClampedWave);
		if (const FWaveDataRow* Row = WaveDataTable->FindRow<FWaveDataRow>(FName(*RowName), TEXT("")))
		{
			FWaveData WaveData;
			WaveData.PlayDuration = Row->PlayDuration;
			WaveData.RestDuration = Row->RestDuration;
			return WaveData;
		}
	}

	const int32 ArrayIndex = ClampedWave - 1;
	if (WaveDataArray.IsValidIndex(ArrayIndex))
	{
		return WaveDataArray[ArrayIndex];
	}

	return GetDefaultWaveData(ClampedWave);
}

// 웨이브 번호 기본 시간값을 반환합니다.
FWaveData UDRPhase3::GetDefaultWaveData(int32 WaveNumber) const
{
	const int32 ClampedWave = FMath::Clamp(WaveNumber, 1, 5);
	FWaveData WaveData;

	switch (ClampedWave)
	{
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	default:
		break;
	}

	return WaveData;
}
// GameBalanceConfig에서 Phase3 공통 값을 로드합니다.
void UDRPhase3::LoadPhase3ConfigFromBalanceConfig()
{
	if (const UGameBalanceConfig* BalanceConfig = UDRAbilitySystemLibrary::GetGameBalanceConfig(GameMode))
	{
		MaxMonsterCount = BalanceConfig->Phase3.MaxMonsterCount;
		DefenseDuration = BalanceConfig->Phase3.DefenseDuration;
		SpawnDistanceMin = BalanceConfig->Phase3.SpawnDistanceMin;
		SpawnDistanceMax = BalanceConfig->Phase3.SpawnDistanceMax;
		PoisonGasSpawnInterval = BalanceConfig->Phase3.PoisonGasSpawnInterval;
		HighLevelEnrageThreshold = BalanceConfig->Phase3.HighLevelEnrageThreshold;
	}
}

// 현재 웨이브 레벨을 증가시키고 범위를 제한합니다.
void UDRPhase3::IncreaseWaveLevel(int32 Amount)
{
	CurrentWaveLevel = FMath::Clamp(CurrentWaveLevel + Amount, 1, 5);
}


// 활성 클렌저 사이트의 속성/델리게이트를 초기화합니다.
void UDRPhase3::InitializeCleanserSite()
{
	// [임시] 1개 사이트 기준으로 검증
	if (ActiveCleanserSites.Num() < 1) return;
	// if (ActiveCleanserSites.Num() != 2) return;

	CleanserSiteHalfHealthTriggered.Empty();

	// [임시] 1개 사이트만 활성화: Second는 First와 동일한 사이트로 처리
	ADRCleanserSite* FirstCleanserSite = ActiveCleanserSites[0].Get();
	ADRCleanserSite* SecondCleanserSite = ActiveCleanserSites.Num() >= 2 ? ActiveCleanserSites[1].Get() : FirstCleanserSite;
	// ADRCleanserSite* FirstCleanserSite = ActiveCleanserSites[0].Get();
	// ADRCleanserSite* SecondCleanserSite = ActiveCleanserSites[1].Get();
	
	for (ADRCleanserSite* Site : ActiveCleanserSites)
	{
		if (!Site || !IsValid(Site)) continue;
		
		CleanserSiteHalfHealthTriggered.Add(Site, false);
		
		if (UAbilitySystemComponent* SiteASC = Site->GetAbilitySystemComponent())
		{
			SiteASC->InitAbilityActorInfo(Site, Site);
			
			Site->InitializeDefaultAttributes();
		}
		
		if (UDRCleanserSiteAttributeSet* CleanserSiteAttributeSet = Site->GetAttributeSet())
		{
			CleanserSiteAttributeSet->OnHealthBelowHalfDelegate.AddDynamic(this, &UDRPhase3::OnCleanserSiteHealthBelowHalf);
			
			CleanserSiteAttributeSet->OnHealthZeroDelegate.AddDynamic(this, &UDRPhase3::OnCleanserSiteHealthZero);
		}
	}

	OnCleanserSiteReadyDelegate.Broadcast(FirstCleanserSite, SecondCleanserSite);
}

// 독가스 스폰 포인트 중 현재 활성 포인트 목록을 계산합니다.
void UDRPhase3::InitializeActiveSpawnPoints()
{
	ActiveSpawnPointIndices.Empty();
	ActiveBlueSpawnPointIndices.Empty();

	FindPoisonGasSpawnPoints();

	if (ActiveCleanserSites.Num() == 0) return;

	TSet<FName> ActiveCleanserIDs;
	for (const TObjectPtr<ADRCleanserSite>& CleanserSite : ActiveCleanserSites)
	{
		if (CleanserSite)
		{
			FName CleanserID = CleanserSite->GetCleanserID();
			if (CleanserID != NAME_None)
			{
				ActiveCleanserIDs.Add(CleanserID);
			}
		}
	}

	for (int32 i = 0; i < AllPoisonGasSpawnPoints.Num(); ++i)
	{
		const FPoisonGasSpawnPointData& Point = AllPoisonGasSpawnPoints[i];

		if (Point.SpawnType == EPoisonGasSpawnPointType::Normal)
		{
			ActiveSpawnPointIndices.Add(i);
		}
		else if (Point.SpawnType == EPoisonGasSpawnPointType::CleanserLinked)
		{
			if (Point.LinkedCleanserTag != NAME_None && ActiveCleanserIDs.Contains(Point.LinkedCleanserTag))
			{
				ActiveSpawnPointIndices.Add(i);
				ActiveBlueSpawnPointIndices.Add(i);
			}
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
		if (Site)
		{
			const FName CleanserID = Site->GetCleanserID();
			if (CleanserID != NAME_None)
			{
				ActiveCleanserIDs.Add(CleanserID);
			}
		}
	}

	const FString CleanserTagPrefixStr = PoisonGasCleanserTagPrefix.ToString();

	// 태그로 마커 액터 탐색
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->ActorHasTag(PoisonGasSpawnPointTag)) continue;

		FPoisonGasSpawnPointData PointData;
		PointData.Location = Actor->GetActorLocation();
		PointData.SpawnType = EPoisonGasSpawnPointType::Normal;
		PointData.LinkedCleanserTag = NAME_None;

		// CleanserLinked 판별: "PoisonGas_CleanserA" 같은 태그가 있는지 확인
		for (const FName& Tag : Actor->Tags)
		{
			const FString TagStr = Tag.ToString();
			if (TagStr.StartsWith(CleanserTagPrefixStr))
			{
				const FString CleanserIDStr = TagStr.RightChop(CleanserTagPrefixStr.Len());
				const FName CleanserIDName = FName(*CleanserIDStr);

				if (ActiveCleanserIDs.Contains(CleanserIDName))
				{
					PointData.SpawnType = EPoisonGasSpawnPointType::CleanserLinked;
					PointData.LinkedCleanserTag = CleanserIDName;
				}
				break;
			}
		}

		AllPoisonGasSpawnPoints.Add(PointData);
	}
}

// 클렌저가 파괴되면 즉시 게임오버를 처리합니다.
void UDRPhase3::OnCleanserSiteDestroyed(ADRCleanserSite* DestroyedSite) const
{
	if (GameMode)
	{
		GameMode->TriggerGameOver();
	}
}

// 클렌저 체력이 절반 이하가 되면 웨이브 레벨을 올립니다.
void UDRPhase3::OnCleanserSiteHealthBelowHalf()
{
	if (!GameMode) return;
	
	IncreaseWaveLevel(1);
	
	if (GameState)
	{
		GameState->SetCurrentWaveLevel(CurrentWaveLevel);
	}
}

// 클렌저 체력이 0이 되면 게임오버를 처리합니다.
void UDRPhase3::OnCleanserSiteHealthZero()
{
	if (!GameMode) return;
	
	GameMode->TriggerGameOver();
}


// 몬스터 수 제한 초과 여부를 검사합니다.
void UDRPhase3::CheckGameOverConditions() const
{
	if (IsMonsterCountExceeded())
	{
		if (GameMode)
		{
			GameMode->TriggerGameOver();
		}
	}
}

// 방어 시간 종료 시 승리/패배 조건을 검사합니다.
void UDRPhase3::CheckVictoryConditions()
{
	bool bAllSitesAlive = true;
	
	for (const TObjectPtr<ADRCleanserSite>& Site : ActiveCleanserSites)
	{
		if (!Site || !IsValid(Site))
		{
			bAllSitesAlive = false;
			break;
		}
	}
	
	if (bAllSitesAlive)
	{
		if (GameMode)
		{
			GameMode->ValidatePhaseCompletion();
		}
	}
	else
	{
		if (GameMode)
		{
			GameMode->TriggerGameOver();
		}
	}
}

// 생존 몬스터 수가 최대치를 넘었는지 확인합니다.
bool UDRPhase3::IsMonsterCountExceeded() const
{
	const int32 CurrentMonsterCount = GetAliveEnemyCount();
	
	return CurrentMonsterCount >= MaxMonsterCount;
}

// 독가스 생성에 사용할 랜덤 포인트 인덱스를 고릅니다.
TArray<int32> UDRPhase3::SelectRandomSpawnPointIndices() const
{
	TArray<int32> SelectedIndices;

	if (ActiveSpawnPointIndices.Num() <= 8)
	{
		return ActiveSpawnPointIndices;
	}

	if (ActiveBlueSpawnPointIndices.Num() > 0)
	{
		int32 RandomBlueIndex = FMath::RandRange(0, ActiveBlueSpawnPointIndices.Num() - 1);
		SelectedIndices.Add(ActiveBlueSpawnPointIndices[RandomBlueIndex]);
	}

	TArray<int32> RemainingPoints = ActiveSpawnPointIndices;

	for (int32 SelectedIdx : SelectedIndices)
	{
		RemainingPoints.Remove(SelectedIdx);
	}

	while (SelectedIndices.Num() < 8 && RemainingPoints.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, RemainingPoints.Num() - 1);
		SelectedIndices.Add(RemainingPoints[RandomIndex]);
		RemainingPoints.RemoveAt(RandomIndex);
	}

	return SelectedIndices;
}

// 독가스 주기 스폰 타이머를 시작합니다.
void UDRPhase3::SpawnToxicGas()
{
	if (!GameMode) return;
    
	UWorld* World = GameMode->GetWorld();
	if (!World) return;
    
	if (PoisonGasSpawnTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(PoisonGasSpawnTimerHandle);
	}
    
	World->GetTimerManager().SetTimer(
		PoisonGasSpawnTimerHandle,
		this,
		&UDRPhase3::SpawnPoisonGasActor,
		PoisonGasSpawnInterval,  // 반복 스폰 주기
		true,   // 반복 실행
		0.0f    // 첫 스폰 지연 시간 (경고 페이즈가 3초 지연을 대신함)
	);
}

// 선택된 포인트에 독가스 액터를 스폰합니다.
void UDRPhase3::SpawnPoisonGasActor()
{
	if (!GameMode || !PoisonGasActorClass) return;

	UWorld* World = GameMode->GetWorld();
	if (!World) return;

	TArray<int32> SelectedIndices = SelectRandomSpawnPointIndices();

	if (SelectedIndices.Num() == 0) return;

	bool bAnySpawned = false;

	for (int32 Index : SelectedIndices)
	{
		if (!AllPoisonGasSpawnPoints.IsValidIndex(Index)) continue;

		const FVector& SpawnLocation = AllPoisonGasSpawnPoints[Index].Location;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ADRPoisonGasActor* PoisonGas = World->SpawnActor<ADRPoisonGasActor>(
			PoisonGasActorClass,
			SpawnLocation,
			FRotator::ZeroRotator,
			SpawnParams
		);

		if (PoisonGas)
		{
			PoisonGas->SetLifeSpan(10.0f);  // 경고 3초 + 활성 7초

			ToxicGasActors.Add(PoisonGas);
			bAnySpawned = true;
		}
	}

	// 배치당 1회 경고 사운드
	if (bAnySpawned)
	{
		if (ADRStageGameState* GS = World->GetGameState<ADRStageGameState>())
		{
			GS->Multicast_PlayPoisonGasWarningSound();
		}
	}
}

// 독가스 타이머와 남아있는 독가스 액터를 정리합니다.
void UDRPhase3::RemoveToxicGas()
{
	if (GameMode)
	{
		if (UWorld* World = GameMode->GetWorld())
		{
			if (PoisonGasSpawnTimerHandle.IsValid())
			{
				World->GetTimerManager().ClearTimer(PoisonGasSpawnTimerHandle);
			}
		}
	}
    
	for (TWeakObjectPtr<AActor> GasActor : ToxicGasActors)
	{
		if (GasActor.IsValid())
		{
			GasActor->Destroy();
		}
	}
    
	ToxicGasActors.Empty();
}

// 엘리트 보스 활성 시 적/아군 태그를 부여합니다.
// 기존 글로벌 태그 시스템은 포효(Roar) 스킬의 범위 기반 오라 버프로 대체됨.
void UDRPhase3::GrantEliteBossTag()
{
}

// 엘리트 보스 비활성 시 부여한 태그를 제거합니다.
// 기존 글로벌 태그 시스템은 포효(Roar) 스킬의 범위 기반 오라 버프로 대체됨.
void UDRPhase3::RemoveEliteBossTag()
{
}

















