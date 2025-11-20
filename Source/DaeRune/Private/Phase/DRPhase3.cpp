// Copyright DaeRune

#include "Phase/DRPhase3.h"

#include "DRGameplayTags.h"
#include "EngineUtils.h"
#include "Character/DREnemy.h"
#include "Actor/DRCleanserSite.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "AbilitySystem/DRCleanserSiteAttributeSet.h"
#include "Character/DRCharacter.h"

UDRPhase3::UDRPhase3()
{
	CurrentWaveNumber = 0;
	CurrentWaveLevel = 1; // 초기 레벨은 1
	CurrentWaveState = EWaveState::Waiting;
	CurrentSpawnCount = 0;
	DefenseStartTime = 0.0f;
	WaveTimeRemaining = 0.0f;
	RestTimeRemaining = 0.0f;
	bEliteBossSpawned = false;
}

void UDRPhase3::OnPhaseStart()
{
	Super::OnPhaseStart();
	
	if (!GameMode || !GameState) return;
	
	// 목표 설정
	SetupPhaseObjective(3);
	
	// GameState Phase3 초기화
	GameState->SetCurrentWaveNumber(0);
	GameState->SetCurrentWaveLevel(0);
	GameState->SetTotalWaves(5);
	
	// 클렌저 사이트 초기화
	InitializeCleanserSite();
	
	// 방어 시간 시작
	if (UWorld* World = GameMode->GetWorld())
	{
		DefenseStartTime = World->GetTimeSeconds();
		World->GetTimerManager().SetTimer(
			DefenseTimerHandle,
			this,
			&UDRPhase3::CheckVictoryConditions,
			DefenseDuration,
			false);
	}
	
	// 첫 웨이브 시작
	StartNextWave();
}

void UDRPhase3::OnPhaseEnd()
{
	Super::OnPhaseEnd();
	
	if (!GameMode) return;
	
	// 모든 타이머 정리
	if (UWorld* World = GameMode->GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaveTimerHandle);
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		World->GetTimerManager().ClearTimer(DefenseTimerHandle);
		World->GetTimerManager().ClearTimer(WaveTimerUpdateHandle);
	}

	// 클렌저 사이트 델리게이트 언바인딩
	for (const TObjectPtr<ADRCleanserSite>& Site : CleanserSites)
	{
		if (Site && IsValid(Site))
		{
			// AttributeSet 델리게이트 언바인딩
			if (UDRCleanserSiteAttributeSet* AttributeSet = Site->GetAttributeSet())
			{
				AttributeSet->OnHealthBelowHalfDelegate.RemoveAll(this);
				AttributeSet->OnHealthZeroDelegate.RemoveAll(this);
			}
        
			// CleanserSite 델리게이트 언바인딩
			Site->OnCleanserSiteDestroyed.RemoveAll(this);
		}
	}
	
	// 유독 가스 제거
	RemoveToxicGas();
}

void UDRPhase3::OnEliteEnemyDeath(AActor* DeadEnemy)
{
	if (!DeadEnemy || !bIsPhaseActive) return;
	
	EliteBosses.Remove(DeadEnemy);

	if (EliteBosses.IsEmpty())
	{
		bEliteBossSpawned = false;
		RemoveEliteBossTag();
	}
}

// ========== 웨이브 시스템 ==========

void UDRPhase3::StartNextWave()
{
	CurrentWaveNumber++;
	
	if (CurrentWaveNumber > 5) return;
	
	// 웨이브 클리어 시 레벨 증가
	if (CurrentWaveNumber > 1)
	{
		IncreaseWaveLevel(1);
	}
	
	// GameState 업데이트
	if (GameState)
	{
		GameState->SetCurrentWaveNumber(CurrentWaveNumber);
		GameState->SetCurrentWaveLevel(CurrentWaveLevel);
		GameState->SetIsWaveRestTime(false);
		GameState->SetWaveRemainingTime(WaveTimeRemaining);
	}
	
	// 웨이브 상태 변경
	CurrentWaveState = EWaveState::InProgress;
	CurrentSpawnCount = 0;
	TotalSpawnCount = 0;
	CurrentSpawnTick = 0;
	
	// 웨이브 데이터 가져오기
	if (WaveDataArray.IsValidIndex(CurrentWaveNumber - 1))
	{
		const FWaveData& CurrentWave = WaveDataArray[CurrentWaveNumber - 1];
		const FWaveLevelModifier& Modifier = GetWaveLevelModifier(CurrentWaveLevel);
		
		// 스폰 간격 계산 (레벨별 수정자 적용)
		float ActualSpawnInterval = CurrentWave.BaseSpawnInterval * Modifier.SpawnIntervalMultiplier;
		TotalSpawnTick = FMath::FloorToInt32(50.f / ActualSpawnInterval);

		// 웨이브 타이머 초기화 및 업데이트 시작
		WaveTimeRemaining = CurrentWave.PlayDuration;
		
		// 스폰 타이머 시작
		if (GameMode)
		{
			if (const UWorld* World = GameMode->GetWorld())
			{
				// 살아있는 플레이어 목록 가져오기
				TArray<AActor*> PlayerCharacters;
				UGameplayStatics::GetAllActorsOfClass(World, ADRCharacter::StaticClass(), PlayerCharacters);
				if (PlayerCharacters.Num() == 0) return;

				TotalSpawnCount = FMath::CeilToInt(CurrentWave.BaseMonstersPerPlayer * PlayerCharacters.Num() * Modifier.MonsterCountMultiplier);
				
				World->GetTimerManager().SetTimer(
					SpawnTimerHandle,
					this,
					&UDRPhase3::ProcessWaveSpawn,
					ActualSpawnInterval,
					true,
					0.0f // 즉시 시작
				);
				
				// 웨이브 종료 타이머
				World->GetTimerManager().SetTimer(
					WaveTimerHandle,
					this,
					&UDRPhase3::EndCurrentWave,
					CurrentWave.PlayDuration,
					false
				);

				// 1초마다 타이머 업데이트
				World->GetTimerManager().SetTimer(
					WaveTimerUpdateHandle,
					[this]()
					{
						WaveTimeRemaining = FMath::Max(0.0f, WaveTimeRemaining - 1.0f);
						if (GameState)
						{
							GameState->SetWaveRemainingTime(WaveTimeRemaining);
						}
					},
					1.0f,
					true,
					0.0f
				);
			}
		}
		
		// 웨이브 레벨별 특수 처리
		if (Modifier.bSpawnToxicGas)
		{
			// 유독 가스 생성 (레벨 4+)
			SpawnToxicGas();
		}
		
		if (Modifier.bSpawnEliteBoss && EliteBossClass)
		{
			// 엘리트 보스 스폰 (레벨 5, 클렌저 사이트 근처)
			if (CleanserSites.Num() > 0 && CleanserSites[0])
			{
				for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				{
					AActor* BossSpawnPoint = *It;
					if (BossSpawnPoint && BossSpawnPoint->ActorHasTag(SpawnPointTag))
					{
						SpawnEliteMonster(BossSpawnPoint->GetActorLocation());
					}
				}
			}
		}
	}
}

void UDRPhase3::EndCurrentWave()
{
	if (!GameMode) return;
	
	// 스폰 타이머 중지
	if (UWorld* World = GameMode->GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}

	// GameState 업데이트
	if (GameState)
	{
		GameState->UpdatePhaseObjectiveProgress(CurrentWaveNumber);
	}
	
	// 휴식 시간 시작
	StartRestTime();
}

void UDRPhase3::StartRestTime()
{
	// 타이머 업데이트 중지
	if (UWorld* World = GameMode->GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaveTimerUpdateHandle);
	}

	// 휴식 시간 초기화
	const FWaveData& CurrentWave = WaveDataArray[CurrentWaveNumber - 1];
	RestTimeRemaining = CurrentWave.RestDuration;

	// GameState에 휴식 시작 알림
	if (GameState)
	{
		GameState->SetIsWaveRestTime(true);
		GameState->SetWaveRemainingTime(RestTimeRemaining);
	}
	
	CurrentWaveState = EWaveState::Rest;
	
	// 휴식 시간 후 다음 웨이브 시작
	if (WaveDataArray.IsValidIndex(CurrentWaveNumber - 1))
	{
		if (GameMode)
		{
			if (const UWorld* World = GameMode->GetWorld())
			{
				World->GetTimerManager().SetTimer(
					WaveTimerUpdateHandle,
					[this]()
					{
						RestTimeRemaining = FMath::Max(0.0f, RestTimeRemaining - 1.0f);
						if (GameState)
						{
							GameState->SetWaveRemainingTime(RestTimeRemaining);
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
}

void UDRPhase3::EndRestTime()
{
	// 다음 웨이브 시작
	StartNextWave();
}

void UDRPhase3::ProcessWaveSpawn()
{
	if (!GameMode) return;
	
	if (!WaveDataArray.IsValidIndex(CurrentWaveNumber - 1)) return;
	
	const FWaveData& CurrentWave = WaveDataArray[CurrentWaveNumber - 1];
	const FWaveLevelModifier& Modifier = GetWaveLevelModifier(CurrentWaveLevel);

	int32 CurrentPlayerCounts = 0;

	const UWorld* World = GameMode->GetWorld();
	if (!World) return;
	
	// 살아있는 플레이어 목록 가져오기
	TArray<AActor*> PlayerCharacters;
	UGameplayStatics::GetAllActorsOfClass(World, ADRCharacter::StaticClass(), PlayerCharacters);
	if (PlayerCharacters.Num() == 0) return;

	CurrentPlayerCounts = PlayerCharacters.Num();

	const int32 SpawnCountLeft = TotalSpawnCount - CurrentSpawnCount;
	const int32 SpawnTickLeft = TotalSpawnTick - CurrentSpawnTick;

	// 플레이어당 총 몬스터 수 계산 (올림 처리)
	const int32 TotalMonstersPerPlayer = FMath::CeilToInt(static_cast<float>(SpawnCountLeft) / static_cast<float>(CurrentPlayerCounts));
	// 회당 스폰 수 = 올림(총 몬스터 / 현재 남은 스폰 횟수)
	const int32 MonstersPerSpawnThisWave = FMath::CeilToInt(static_cast<float>(TotalMonstersPerPlayer) / SpawnTickLeft);
	
	// 최대 스폰 횟수 체크
	if (SpawnCountLeft == 0)
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		return;
	}
	
	// 몬스터 스폰
	SpawnMonstersAroundPlayers(PlayerCharacters, MonstersPerSpawnThisWave);
	
	CurrentSpawnTick++;
}

// ========== 스폰 시스템 ==========

void UDRPhase3::SpawnMonstersAroundPlayers(const TArray<AActor*>& PlayerCharacters, int32 MonstersPerSpawnThisWave)
{
	if (!GameMode) return;
	
	UWorld* World = GameMode->GetWorld();
	if (!World) return;
	
	if (!NormalMonsterClass) return;
	
	if (PlayerCharacters.Num() == 0) return;
	
	// 웨이브 데이터 가져오기
	if (!WaveDataArray.IsValidIndex(CurrentWaveNumber - 1)) return;
	
	// 각 플레이어 주변에 몬스터 스폰
	for (const AActor* Player : PlayerCharacters)
	{
		for (int32 i = 0; i < MonstersPerSpawnThisWave; i++)
		{
			// 스폰 위치 계산
			FVector SpawnLocation = CalculateSpawnLocation(Player->GetActorLocation());
		
			// 웨이브 레벨에 따라 몬스터 클래스 선택 (확률 기반)
			TSubclassOf<ADREnemy> SelectedMonsterClass = SelectMonsterClass(CurrentWaveLevel);
		
			if (!SelectedMonsterClass) continue;
		
			// SpawnActorDeferred 사용으로 BeginPlay 전에 레벨 설정
			ADREnemy* SpawnedEnemy = World->SpawnActorDeferred<ADREnemy>(
				SelectedMonsterClass,
				FTransform(FRotator::ZeroRotator, SpawnLocation),
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
			);
		
			if (SpawnedEnemy)
			{
				// BeginPlay 호출 전에 레벨 설정
				SpawnedEnemy->SetLevel(CurrentWaveLevel);

				// 페이즈3 적으로 마킹
				SpawnedEnemy->bIsPhase3Enemy = true;
				if (CurrentWaveLevel >= 4)
				{
					SpawnedEnemy->EnrageHealthThreshold = 0.25f;
				}
				
				// FinishSpawning으로 BeginPlay 실행
				SpawnedEnemy->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));
				
				CurrentSpawnCount++;

				// 델리게이트 바인딩
				if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(SpawnedEnemy))
				{
					CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UDRPhaseBase::OnEnemyDeath);
				}

				if (bEliteBossSpawned)
				{
					if (UAbilitySystemComponent* ASC = SpawnedEnemy->GetAbilitySystemComponent())
					{
						ASC->AddLooseGameplayTag(FDRGameplayTags::Get().Buff_Elite);
					}
				}
				
				// PhaseBase의 SpawnedEnemies 배열에 추가
				SpawnedEnemies.Add(SpawnedEnemy);

				CheckGameOverConditions();

				if (TotalSpawnCount == CurrentSpawnCount) return;
			}
		}
	}
}

FVector UDRPhase3::CalculateSpawnLocation(const FVector& PlayerLocation) const
{
	if (!GameMode) return PlayerLocation;
	
	UWorld* World = GameMode->GetWorld();
	if (!World) return PlayerLocation;
	
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetNavigationSystem(World);
	if (!NavSys)
	{
		return PlayerLocation + FVector(FMath::RandRange(-1000.f, 1000.f), FMath::RandRange(-1000.f, 1000.f), 0.f);
	}
	
	// 플레이어로부터 일정 거리 떨어진 랜덤 위치
	float SpawnDistance = FMath::RandRange(SpawnDistanceMin, SpawnDistanceMax);
	FVector RandomDirection = FVector(FMath::RandRange(-1.f, 1.f), FMath::RandRange(-1.f, 1.f), 0.f).GetSafeNormal();
	FVector DesiredLocation = PlayerLocation + RandomDirection * SpawnDistance;
	
	// 네비게이션 메쉬 상의 유효한 위치 찾기
	FNavLocation NavLocation;
	if (NavSys->ProjectPointToNavigation(DesiredLocation, NavLocation, FVector(500.f, 500.f, 500.f)))
	{
		return NavLocation.Location;
	}
	
	return DesiredLocation;
}

TSubclassOf<ADREnemy> UDRPhase3::SelectMonsterClass(int32 WaveLevel) const
{
	const FWaveLevelModifier& Modifier = GetWaveLevelModifier(WaveLevel);
	
	// 확률 계산 (0.0 ~ 1.0)
	float RandomValue = FMath::FRand();
	
	// 은신형 몬스터 체크 (우선순위 높음)
	if (RandomValue < Modifier.StealthMonsterSpawnChance && StealthMonsterClass)
	{
		return StealthMonsterClass;
	}
	
	// 돌진형 몬스터 체크
	if (RandomValue < Modifier.RushMonsterSpawnChance && RushMonsterClass)
	{
		return RushMonsterClass;
	}
	
	// 기본은 일반 몬스터
	return NormalMonsterClass;
}

void UDRPhase3::SpawnEliteMonster(const FVector& SpawnLocation)
{
	if (!GameMode) return;

	TSubclassOf<ADREnemy> MonsterClass = EliteBossClass;
	
	UWorld* World = GameMode->GetWorld();
	if (!World) return;
	
	// SpawnActorDeferred 사용으로 BeginPlay 전에 레벨 설정 가능
	ADREnemy* SpawnedEnemy = World->SpawnActorDeferred<ADREnemy>(
		MonsterClass,
		FTransform(FRotator::ZeroRotator, SpawnLocation),
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
	);
	
	if (SpawnedEnemy)
	{
		// 레벨 설정
		SpawnedEnemy->SetLevel(CurrentWaveLevel);

		// 페이즈3 적으로 마킹
		SpawnedEnemy->bIsPhase3Enemy = true;
		if (CurrentWaveLevel >= 4)
		{
			SpawnedEnemy->EnrageHealthThreshold = 0.25f;
		}
		
		// FinishSpawning
		SpawnedEnemy->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));
		
		// 엘리트 보스는 추가로 참조 저장
		EliteBosses.Add(SpawnedEnemy);

		if (!bEliteBossSpawned)
		{
			bEliteBossSpawned = true;
			GrantEliteBossTag();
		}

		// 델리게이트 바인딩
		if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(SpawnedEnemy))
		{
			CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UDRPhase3::OnEliteEnemyDeath);
		}
	}
}

// ========== 웨이브 레벨 시스템 ==========

FWaveLevelModifier UDRPhase3::GetWaveLevelModifier(int32 WaveLevel) const
{
	// 웨이브 레벨은 1~5로 제한
	int32 ClampedLevel = FMath::Clamp(WaveLevel, 1, 5);
	
	if (const FWaveLevelModifier* Modifier = WaveLevelModifiers.Find(ClampedLevel))
	{
		return *Modifier;
	}
	
	// 기본값 반환
	return FWaveLevelModifier();
}

void UDRPhase3::IncreaseWaveLevel(int32 Amount)
{
	CurrentWaveLevel = FMath::Clamp(CurrentWaveLevel + Amount, 1, 5);
}

// ========== 클렌저 사이트 ==========

void UDRPhase3::InitializeCleanserSite()
{
	// 활성 사이트가 2개인지 확인
	if (ActiveCleanserSites.Num() != 2) return;
	
	// CleanserSites 배열 초기화
	CleanserSiteHalfHealthTriggered.Empty();

	ADRCleanserSite* FirstCleanserSite = ActiveCleanserSites[0].Get();
	ADRCleanserSite* SecondCleanserSite = ActiveCleanserSites[1].Get();
	
	for (ADRCleanserSite* Site : ActiveCleanserSites)
	{
		if (!Site || !IsValid(Site)) continue;
		
		// 체력 추적 초기화 (50% 트리거 플래그)
		CleanserSiteHalfHealthTriggered.Add(Site, false);
		
		// GAS 초기화
		if (UAbilitySystemComponent* SiteASC = Site->GetAbilitySystemComponent())
		{
			// AbilityActorInfo 초기화
			SiteASC->InitAbilityActorInfo(Site, Site);
			
			// 기본 Attributes 초기화 (GE로 체력 설정)
			Site->InitializeDefaultAttributes();
		}
		
		// AttributeSet 가져오기 및 델리게이트 바인딩
		if (UDRCleanserSiteAttributeSet* CleanserSiteAttributeSet = Site->GetAttributeSet())
		{
			// 체력 50% 이하 델리게이트 바인딩
			CleanserSiteAttributeSet->OnHealthBelowHalfDelegate.AddDynamic(this, &UDRPhase3::OnCleanserSiteHealthBelowHalf);
			
			// 체력 0 델리게이트 바인딩
			CleanserSiteAttributeSet->OnHealthZeroDelegate.AddDynamic(this, &UDRPhase3::OnCleanserSiteHealthZero);
		}
	}

	OnCleanserSiteReadyDelegate.Broadcast(FirstCleanserSite, SecondCleanserSite);
}

void UDRPhase3::OnCleanserSiteDestroyed(ADRCleanserSite* DestroyedSite) const
{
	// 클렌저 사이트가 하나라도 파괴되면 게임 오버
	if (GameMode)
	{
		GameMode->TriggerGameOver();
	}
}

void UDRPhase3::OnCleanserSiteHealthBelowHalf()
{
	if (!GameMode) return;
	
	// 웨이브 레벨 1 증가
	IncreaseWaveLevel(1);
	
	// GameState 업데이트
	if (GameState)
	{
		GameState->SetCurrentWaveLevel(CurrentWaveLevel);
	}
}

void UDRPhase3::OnCleanserSiteHealthZero()
{
	if (!GameMode) return;
	
	// 게임 오버 처리
	GameMode->TriggerGameOver();
}

// ========== 게임 오버/승리 조건 ==========

void UDRPhase3::CheckGameOverConditions() const
{
	// 몬스터 수가 100마리 초과하면 게임 오버
	if (IsMonsterCountExceeded())
	{
		if (GameMode)
		{
			GameMode->TriggerGameOver();
		}
	}
}

void UDRPhase3::CheckVictoryConditions()
{
	// 방어 시간이 끝났고, 모든 클렌저 사이트가 살아있으면 승리
	bool bAllSitesAlive = true;
	
	for (const TObjectPtr<ADRCleanserSite>& Site : CleanserSites)
	{
		if (!Site || !IsValid(Site))
		{
			bAllSitesAlive = false;
			break;
		}
	}
	
	if (bAllSitesAlive)
	{
		// Phase 완료
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

bool UDRPhase3::IsMonsterCountExceeded() const
{
	// PhaseBase의 GetAliveEnemyCount 사용
	const int32 CurrentMonsterCount = GetAliveEnemyCount();
	
	return CurrentMonsterCount >= MaxMonsterCount;
}

// ========== 환경 위협 ==========

void UDRPhase3::SpawnToxicGas()
{
	// TODO: 유독 가스 액터 스폰
	// 맵 전역에 일정 간격으로 유독 가스 배치
}

void UDRPhase3::RemoveToxicGas()
{
	// 모든 유독 가스 제거
	for (TWeakObjectPtr<AActor> GasActor : ToxicGasActors)
	{
		if (GasActor.IsValid())
		{
			GasActor->Destroy();
		}
	}
	
	ToxicGasActors.Empty();
}

void UDRPhase3::GrantEliteBossTag()
{
	if (!bEliteBossSpawned) return;

	for (TWeakObjectPtr<AActor>& EnemyPtr : SpawnedEnemies)
	{
		if (!EnemyPtr.IsValid()) continue;

		ADREnemy* Enemy = Cast<ADREnemy>(EnemyPtr.Get());
		if (!Enemy) continue;

		if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTag(FDRGameplayTags::Get().Buff_Elite);
		}
	}
	
	if (const UWorld* World = GameMode->GetWorld())
	{
		// 살아있는 플레이어 목록 가져오기
		TArray<AActor*> PlayerCharacters;
		UGameplayStatics::GetAllActorsOfClass(World, ADRCharacter::StaticClass(), PlayerCharacters);
		if (PlayerCharacters.Num() == 0) return;

		for (AActor* Player : PlayerCharacters)
		{
			ADRCharacter* PlayerCharacter = Cast<ADRCharacter>(Player);
			if (!PlayerCharacter) continue;

			if (UAbilitySystemComponent* ASC = PlayerCharacter->GetAbilitySystemComponent())
			{
				ASC->AddLooseGameplayTag(FDRGameplayTags::Get().Debuff_Elite);
			}
		}
	}

	for (const TObjectPtr<ADRCleanserSite>& Site : CleanserSites)
	{
		if (!Site || !IsValid(Site)) return;

		if (UAbilitySystemComponent* ASC = Site->GetAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTag(FDRGameplayTags::Get().Debuff_Elite);
		}
	}
}

void UDRPhase3::RemoveEliteBossTag()
{
	if (bEliteBossSpawned) return;

	for (TWeakObjectPtr<AActor>& EnemyPtr : SpawnedEnemies)
	{
		if (!EnemyPtr.IsValid()) continue;

		ADREnemy* Enemy = Cast<ADREnemy>(EnemyPtr.Get());
		if (!Enemy) continue;

		if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(FDRGameplayTags::Get().Buff_Elite);
		}
	}
	
	if (const UWorld* World = GameMode->GetWorld())
	{
		// 살아있는 플레이어 목록 가져오기
		TArray<AActor*> PlayerCharacters;
		UGameplayStatics::GetAllActorsOfClass(World, ADRCharacter::StaticClass(), PlayerCharacters);
		if (PlayerCharacters.Num() == 0) return;

		for (AActor* Player : PlayerCharacters)
		{
			ADRCharacter* PlayerCharacter = Cast<ADRCharacter>(Player);
			if (!PlayerCharacter) continue;

			if (UAbilitySystemComponent* ASC = PlayerCharacter->GetAbilitySystemComponent())
			{
				ASC->RemoveLooseGameplayTag(FDRGameplayTags::Get().Debuff_Elite);
			}
		}
	}

	for (const TObjectPtr<ADRCleanserSite>& Site : CleanserSites)
	{
		if (!Site || !IsValid(Site)) return;

		if (UAbilitySystemComponent* ASC = Site->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(FDRGameplayTags::Get().Debuff_Elite);
		}
	}
}
