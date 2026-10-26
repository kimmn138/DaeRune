// Copyright DaeRune


#include "Phase/DRPhase1.h"

#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Actor/DRCleanserSite.h"
#include "Actor/DRDoorManager.h"
#include "Actor/DREnemySpawnGroup.h"
#include "Character/DREnemy.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Interaction/CombatInterface.h"
#include "DaeRune/DRLogChannels.h"

void UDRPhase1::OnPhaseStart()
{
	Super::OnPhaseStart();

	if (!GameMode || !GameState) return;

	// 1) 페이즈 목표 / GameState 초기화
	SetupPhaseObjective(1);

	GameState->SetCollectedParts(0);
	GameState->SetCleanserActivated(false);

	GameState->SetInitialPlayerCount(GameState->GetAlivePlayers().Num());

	// 2) 클렌저 사이트 1개 강제 유지 + 활성화 + 부품 설치 델리게이트 구독
	KeepSingleCleanserSite();
	if (!ActiveSite.IsValid())
	{
		UE_LOG(LogDR, Error, TEXT("[Phase1] ActiveSite is invalid after KeepSingleCleanserSite()."));
		return;
	}

	ActiveSite->ActivateSite();
	ActiveSite->OnPartInstalled.AddDynamic(this, &UDRPhase1::OnPartInstalled);

	// 3) 사이트 주변 적 스폰 (배열 기반)
	SpawnEnemiesAroundCleanserSite(ActiveSite.Get());

	// 4) 통로 DREnemySpawnGroup 자동 수집 + 그룹별 부품 운반자 선정
	// 주의: 이 페이즈는 GameMode::BeginPlay 안에서 동기 실행되는데, 레벨에 배치된
	// DREnemySpawnGroup 액터들의 BeginPlay(= PrePlacedEnemies 등록)는 UE의 액터 BeginPlay
	// 순서 보장이 없어 아직 실행되지 않았을 수 있다. 이 시점에 바로 수집하면 RegisteredEnemies가
	// 비어 있어 부품 운반자 선정 및 Phase1 태그 부여가 모두 실패한다.
	// → 모든 액터 BeginPlay가 끝난 다음 틱으로 지연시킨다. (bCarriesPart는 복제 + OnRep로
	//    동기화되므로 1프레임 지연은 시각적으로 무해하다.)
	if (UWorld* World = GameMode->GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			CollectSpawnGroups();
			AssignPartCarriersForAllGroups();
		}));
	}
	else
	{
		// 월드를 못 얻는 예외 상황: 즉시 실행(기존 동작 폴백)
		CollectSpawnGroups();
		AssignPartCarriersForAllGroups();
	}

	// 5) GameState 진행률 초기화 (CleanserSite::RequiredPartsCount = 2 고정)
	FPhaseObjectiveData PhaseObjective = GameState->GetCurrentPhaseObjective();
	PhaseObjective.RequiredCount = RequiredPartsToComplete;
	GameState->SetPhaseObjective(PhaseObjective);
	GameState->UpdatePhaseObjectiveProgress(0);
}

bool UDRPhase1::IsCompleted() const
{
	// 부품 설치 완료 + 클렌저 활성화 시 페이즈 완료
	if (!GameState) return false;
	return GameState->GetCollectedParts() >= RequiredPartsToComplete && GameState->IsCleanserActivated();
}

void UDRPhase1::OnPhaseEnd()
{
	// 부품 설치 델리게이트 해제
	if (ActiveSite.IsValid())
	{
		ActiveSite->OnPartInstalled.RemoveDynamic(this, &UDRPhase1::OnPartInstalled);
	}

	// DoorManager에 알림 (기존 Phase1과 동일한 처리)
	if (ADRDoorManager* DoorMgr = GetDoorManager())
	{
		DoorMgr->OnPhase1Ended();
	}

	// 컬렉션 정리
	SpawnGroups.Empty();
	PartCarrierByGroup.Empty();
	ActiveSite.Reset();

	Super::OnPhaseEnd();
}

void UDRPhase1::KeepSingleCleanserSite()
{
	if (CleanserSites.Num() == 0)
	{
		UE_LOG(LogDR, Error, TEXT("[Phase1] No CleanserSite found in the level."));
		return;
	}

	if (CleanserSites.Num() > 1)
	{
		UE_LOG(LogDR, Warning, TEXT("[Phase1] %d CleanserSites found. Keeping index 0, destroying rest."), CleanserSites.Num());
	}

	const int32 IndexToKeep = 0;
	for (int32 i = CleanserSites.Num() - 1; i >= 0; --i)
	{
		if (i == IndexToKeep) continue;

		TObjectPtr<ADRCleanserSite> SiteToDestroy = CleanserSites[i];
		if (SiteToDestroy && IsValid(SiteToDestroy))
		{
			SiteToDestroy->Destroy();
		}
		CleanserSites.RemoveAt(i);
	}

	// 활성 사이트 = 남은 사이트 1개
	TArray<TObjectPtr<ADRCleanserSite>> SelectedSites = CleanserSites;
	SetActiveCleanserSites(SelectedSites);
	if (GameState)
	{
		TArray<ADRCleanserSite*> RawSites;
		RawSites.Reserve(SelectedSites.Num());
		for (const TObjectPtr<ADRCleanserSite>& Site : SelectedSites)
		{
			if (Site) RawSites.Add(Site.Get());
		}
		GameState->SetCleanserSites(RawSites);
	}

	ActiveSite = CleanserSites.IsValidIndex(0) ? CleanserSites[0] : nullptr;
}

void UDRPhase1::SpawnEnemiesAroundCleanserSite(ADRCleanserSite* Site)
{
	if (!Site) return;

	const TArray<FVector> Locations = Site->GetPhase1EnemySpawnLocations();
	const int32 SpawnCount = FMath::Min(EnemiesToSpawn.Num(), Locations.Num());

	if (EnemiesToSpawn.Num() != Locations.Num())
	{
		UE_LOG(LogDR, Warning,
			TEXT("[Phase1] EnemiesToSpawn (%d) and CleanserSite spawn locations (%d) mismatch. Spawning %d."),
			EnemiesToSpawn.Num(), Locations.Num(), SpawnCount);
	}

	for (int32 i = 0; i < SpawnCount; ++i)
	{
		ADREnemy* Spawned = SpawnEnemyDeferred(EnemiesToSpawn[i], FTransform(Locations[i]));
		if (!Spawned) continue;

		TrackSpawnedEnemy(Spawned);
		ApplyPhase1Tag(Spawned);
	}
}

void UDRPhase1::CollectSpawnGroups()
{
	SpawnGroups.Empty();

	UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!World) return;

	for (TActorIterator<ADREnemySpawnGroup> It(World); It; ++It)
	{
		if (ADREnemySpawnGroup* Group = *It)
		{
			SpawnGroups.Add(Group);

			// 그룹에 등록된 모든 적에게 Phase1 태그 부여
			for (ADREnemy* Enemy : Group->GetRegisteredEnemies())
			{
				ApplyPhase1Tag(Enemy);
			}
		}
	}
}

void UDRPhase1::AssignPartCarriersForAllGroups()
{
	PartCarrierByGroup.Empty();

	if (!ActiveSite.IsValid()) return;

	// 그룹 하나당 부품 하나 — 그룹은 정확히 RequiredPartsToComplete 개를 권장한다
	if (SpawnGroups.Num() != RequiredPartsToComplete)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[Phase1] SpawnGroup count (%d) does not match RequiredPartsToComplete (%d). Will assign carriers to first %d groups."),
			SpawnGroups.Num(), RequiredPartsToComplete, FMath::Min(SpawnGroups.Num(), RequiredPartsToComplete));
	}

	const int32 MaxCarrierGroups = FMath::Min(SpawnGroups.Num(), RequiredPartsToComplete);
	for (int32 i = 0; i < MaxCarrierGroups; ++i)
	{
		ADREnemySpawnGroup* Group = SpawnGroups[i];
		if (!Group) continue;

		ADREnemy* Carrier = PickPartCarrierFromGroup(Group);
		if (Carrier)
		{
			ConfigurePartCarrier(Carrier);
			Group->SetPartCarrierEnemy(Carrier);
			PartCarrierByGroup.Add(Group, Carrier);
		}
	}
}

ADREnemy* UDRPhase1::PickPartCarrierFromGroup(ADREnemySpawnGroup* Group) const
{
	if (!Group) return nullptr;

	const TArray<ADREnemy*> Registered = Group->GetRegisteredEnemies();

	TArray<ADREnemy*> Candidates;
	Candidates.Reserve(Registered.Num());
	for (ADREnemy* Enemy : Registered)
	{
		if (!Enemy || !IsValid(Enemy)) continue;
		if (ArmadilloEnemyClass && Enemy->IsA(ArmadilloEnemyClass)) continue;
		Candidates.Add(Enemy);
	}

	if (Candidates.Num() == 0)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[Phase1] SpawnGroup %s has no non-Armadillo carrier candidates. Skipping part assignment."),
			*Group->GetName());
		return nullptr;
	}

	const int32 PickedIndex = FMath::RandRange(0, Candidates.Num() - 1);
	return Candidates[PickedIndex];
}

void UDRPhase1::ConfigurePartCarrier(ADREnemy* Carrier) const
{
	if (!Carrier) return;

	// PartActorClass 주입 (DropPart()에서 사용)
	if (PartActorClass)
	{
		Carrier->PartActorClass = PartActorClass;
	}

	// bCarriesPart=true + PartMesh 가시화 (서버 즉시 + OnRep로 클라이언트 동기화)
	Carrier->SetCarriesPart(true);

	// 부품 운반자는 이동속도 50% 감속. MoveSpeed 어트리뷰트 베이스를 직접 조정하면
	// UDRAttributeSet::PostAttributeChange에서 CharacterMovement::MaxWalkSpeed가 자동 동기화됨.
	// ★현재값(일시 버프·감속 포함)이 아니라 베이스값을 절반으로 한다 — 그 순간 걸려 있던 효과가 베이스에 굳지 않게
	if (UAbilitySystemComponent* ASC = Carrier->GetAbilitySystemComponent())
	{
		const FGameplayAttribute MoveSpeedAttr = UDRAttributeSet::GetMoveSpeedAttribute();
		ASC->SetNumericAttributeBase(MoveSpeedAttr, ASC->GetNumericAttributeBase(MoveSpeedAttr) * CarrierMoveSpeedRatio);
	}
}

void UDRPhase1::ApplyPhase1Tag(ADREnemy* Enemy) const
{
	if (!Enemy) return;

	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	if (!ASC) return;

	const FGameplayTag TagToApply = Phase1EnemyTag.IsValid()
		? Phase1EnemyTag
		: FDRGameplayTags::Get().State_Enemy_Phase1;

	// 서버 권위 ExecCalc가 서버에서만 실행되므로 LooseTag만으로 충분.
	// 추후 클라이언트 UI에서 태그 조회가 필요하면 AddReplicatedLooseGameplayTag로 전환.
	ASC->AddLooseGameplayTag(TagToApply);
}

void UDRPhase1::OnPartInstalled(ADRCleanserSite* Site)
{
	if (!Site || !GameState) return;

	const int32 Installed = Site->GetInstalledPartsCount();
	GameState->SetCollectedParts(Installed);
	GameState->UpdatePhaseObjectiveProgress(Installed);

	if (Site->IsPartInstallationComplete())
	{
		GameState->SetCleanserActivated(true);
		if (GameMode)
		{
			GameMode->ValidatePhaseCompletion();
		}
	}
}

ADRDoorManager* UDRPhase1::GetDoorManager()
{
	if (CachedDoorManager) return CachedDoorManager;

	if (UWorld* World = GetWorld())
	{
		if (ADRStageGameState* StageGameState = World->GetGameState<ADRStageGameState>())
		{
			CachedDoorManager = StageGameState->GetDoorManager();
		}
	}

	return CachedDoorManager;
}
