// Copyright DaeRune


#include "Tutorial/DRTutorialManager.h"
#include "Tutorial/DRTutorialGate.h"
#include "Tutorial/DRTutorialPressurePlate.h"
#include "Tutorial/DRTutorialStartTile.h"
#include "Character/DREnemy.h"
#include "Character/DRCharacter.h"
#include "Actor/DRCleanserSite.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "DRGameplayTags.h"
#include "UI/HUD/DRHUD.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "Game/DRTutorialGameMode.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "AbilitySystemInterface.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Kismet/GameplayStatics.h"

ADRTutorialManager::ADRTutorialManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
}

void ADRTutorialManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority()) return;

	// 구간 1은 조건 없이 클리어 가능 (발판에 도달하면 바로 통과)
	bSection1Cleared = true;

	// 샌드백 적 설정
	if (DummyEnemy_Center)
	{
		SetupDummyEnemy(DummyEnemy_Center);
	}

	// 부품 적 AI 비활성화 (시작 타일 밟기 전까지)
	StopPartEnemyAI();

	// 클렌저사이트 델리게이트 바인딩
	if (TutorialCleanserSite)
	{
		TutorialCleanserSite->ActivateSite();
		TutorialCleanserSite->OnPartInstalled.AddDynamic(this, &ADRTutorialManager::OnPartInstalledToSite);
	}

	// 초기 UI 설정 (약간의 딜레이로 HUD 초기화 대기)
	FTimerHandle InitUITimer;
	GetWorldTimerManager().SetTimer(InitUITimer, [this]()
	{
		UpdateObjectiveUI(
			NSLOCTEXT("Tutorial", "Obj_Section1", "장애물을 넘어 이동하세요"),
			FText::GetEmpty(), 0, 0
		);
	}, 0.5f, false);
}

void ADRTutorialManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority()) return;

	// 목표 2: WaterPump 활성 시간 누적
	if (CurrentObjective != ECombatObjective::Objective2_WaterPump) return;

	UDRAbilitySystemComponent* ASC = GetPlayerASC();
	if (!ASC) return;

	static const FGameplayTag RMBTag = FGameplayTag::RequestGameplayTag(FName("InputTag.RMB"));
	FGameplayAbilitySpec* WaterPumpSpec = ASC->FindAbilitySpecByInputTag(RMBTag);

	if (WaterPumpSpec && WaterPumpSpec->IsActive())
	{
		WaterPumpAccumulatedTime += DeltaTime;

		UpdateObjectiveUI(
			NSLOCTEXT("Tutorial", "Obj2", "물대포를 발사하세요 (우클릭 유지)"),
			NSLOCTEXT("Tutorial", "Obj2Fmt", "초"),
			FMath::FloorToInt(WaterPumpAccumulatedTime),
			FMath::FloorToInt(RequiredWaterPumpSeconds)
		);

		if (WaterPumpAccumulatedTime >= RequiredWaterPumpSeconds)
		{
			TransitionToObjective(ECombatObjective::Objective3_SeedCannon);
		}
	}
}

// ========== 구간 진행 ==========

void ADRTutorialManager::OnPressurePlateActivated(int32 SectionIndex)
{
	if (!HasAuthority()) return;

	switch (SectionIndex)
	{
	case 1:
		if (bSection1Cleared)
		{
			if (PressurePlate1) PressurePlate1->MarkActivated();
			if (Gate1) Gate1->OpenGate();
			TransitionToSection(ETutorialSection::Section2_Combat);
		}
		break;

	case 2:
		if (bSection2Cleared)
		{
			if (PressurePlate2) PressurePlate2->MarkActivated();
			if (Gate2) Gate2->OpenGate();
			TransitionToSection(ETutorialSection::Section3_PartCollect);
		}
		break;

	case 3:
		if (bSection3Cleared)
		{
			if (PressurePlate3) PressurePlate3->MarkActivated();
			if (ADRTutorialGameMode* GM = Cast<ADRTutorialGameMode>(GetWorld()->GetAuthGameMode()))
			{
				GM->TriggerTutorialComplete();
			}
		}
		break;

	default:
		break;
	}
}

void ADRTutorialManager::TransitionToSection(ETutorialSection NewSection)
{
	CurrentSection = NewSection;

	switch (NewSection)
	{
	case ETutorialSection::Section2_Combat:
		// 구간 2 진입 시 첫 목표(ClawSwipe)부터 시작 (어빌리티는 각 목표에서 개별 부여)
		TransitionToObjective(ECombatObjective::Objective1_MeleeAttack);
		break;

	case ETutorialSection::Section3_PartCollect:
		CurrentObjective = ECombatObjective::None;
		UpdateObjectiveUI(
			NSLOCTEXT("Tutorial", "Obj_Section3", "시작 타일을 밟아 부품 수집을 시작하세요"),
			FText::GetEmpty(), 0, 0
		);
		break;

	case ETutorialSection::Completed:
		break;

	default:
		break;
	}
}

// ========== 전투 목표 ==========

void ADRTutorialManager::TransitionToObjective(ECombatObjective NewObjective)
{
	CurrentObjective = NewObjective;

	switch (NewObjective)
	{
	case ECombatObjective::Objective1_MeleeAttack:
		// ClawSwipe 어빌리티 부여 (UI에 아이콘 표시)
		GrantAbilityToPlayer(ClawSwipeAbilityClass);
		MeleeHitCount = 0;
		UpdateObjectiveUI(
			NSLOCTEXT("Tutorial", "Obj1", "기본 공격으로 적을 공격하세요 (좌클릭)"),
			NSLOCTEXT("Tutorial", "Obj1Fmt", "적중"),
			0, RequiredMeleeHits
		);
		break;

	case ECombatObjective::Objective2_WaterPump:
		// WaterPump 어빌리티 부여 (UI에 아이콘 표시)
		GrantAbilityToPlayer(WaterPumpAbilityClass);
		WaterPumpAccumulatedTime = 0.f;
		UpdateObjectiveUI(
			NSLOCTEXT("Tutorial", "Obj2", "물대포를 발사하세요 (우클릭 유지)"),
			NSLOCTEXT("Tutorial", "Obj2Fmt", "초"),
			0, FMath::FloorToInt(RequiredWaterPumpSeconds)
		);
		break;

	case ECombatObjective::Objective3_SeedCannon:
		// SeedCannon 어빌리티 부여 (UI에 아이콘 표시)
		GrantAbilityToPlayer(SeedCannonAbilityClass);
		SpawnSeedCannonDummies();
		UpdateObjectiveUI(
			NSLOCTEXT("Tutorial", "Obj3", "시드캐논으로 모든 적을 한 번에 맞추세요 (Q)"),
			NSLOCTEXT("Tutorial", "Obj3Fmt", "동시 적중"),
			0, RequiredSimultaneousHits
		);
		break;

	case ECombatObjective::AllComplete:
		bSection2Cleared = true;
		CleanupSeedCannonDummies();
		UpdateObjectiveUI(
			NSLOCTEXT("Tutorial", "Obj_CombatDone", "전투 훈련 완료! 발판을 밟으세요"),
			FText::GetEmpty(), 0, 0
		);
		break;

	default:
		break;
	}
}

void ADRTutorialManager::ReportDamageHit(const FGameplayTagContainer& AbilityTags)
{
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();

	// 목표 1: ClawSwipe 적중만 카운트 (AbilityTag로 구분)
	if (CurrentObjective == ECombatObjective::Objective1_MeleeAttack)
	{
		if (!AbilityTags.HasTag(GameplayTags.Abilities_GardenRobot_ClawSwipe)) return;

		MeleeHitCount++;
		UpdateObjectiveUI(
			NSLOCTEXT("Tutorial", "Obj1", "기본 공격으로 적을 공격하세요 (좌클릭)"),
			NSLOCTEXT("Tutorial", "Obj1Fmt", "적중"),
			MeleeHitCount, RequiredMeleeHits
		);

		if (MeleeHitCount >= RequiredMeleeHits)
		{
			TransitionToObjective(ECombatObjective::Objective2_WaterPump);
		}
	}
}

void ADRTutorialManager::ReportSeedCannonHits(int32 HitCount)
{
	if (CurrentObjective != ECombatObjective::Objective3_SeedCannon) return;

	if (HitCount >= RequiredSimultaneousHits)
	{
		TransitionToObjective(ECombatObjective::AllComplete);
	}
	else
	{
		// 실패 시 현재 적중 수 표시 (재시도 유도)
		UpdateObjectiveUI(
			NSLOCTEXT("Tutorial", "Obj3", "시드캐논으로 모든 적을 한 번에 맞추세요 (Q)"),
			NSLOCTEXT("Tutorial", "Obj3Fmt", "동시 적중"),
			HitCount, RequiredSimultaneousHits
		);
	}
}

// ========== 구간 3 부품 수집 ==========

void ADRTutorialManager::OnStartTileActivated()
{
	if (!HasAuthority()) return;

	StartPartEnemyAI();

	UpdateObjectiveUI(
		NSLOCTEXT("Tutorial", "Obj_Parts", "부품을 들고 있는 적을 잡아 부품을 수집하세요"),
		FText::GetEmpty(), 0, 0
	);
}

void ADRTutorialManager::OnPartInstalledToSite(ADRCleanserSite* Site)
{
	if (!Site) return;

	int32 Installed = Site->GetInstalledPartsCount();
	UpdateObjectiveUI(
		NSLOCTEXT("Tutorial", "Obj_Install", "부품을 클렌저사이트에 설치하세요"),
		NSLOCTEXT("Tutorial", "Obj_InstallFmt", "설치 완료"),
		Installed, 2
	);

	if (Site->IsPartInstallationComplete())
	{
		bSection3Cleared = true;
		UpdateObjectiveUI(
			NSLOCTEXT("Tutorial", "Obj_AllDone", "튜토리얼 완료! 발판을 밟으세요"),
			FText::GetEmpty(), 0, 0
		);
	}
}

// ========== 어빌리티 부여 ==========

void ADRTutorialManager::GrantAbilityToPlayer(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilityClass) return;

	UDRAbilitySystemComponent* ASC = GetPlayerASC();
	if (!ASC) return;

	FGameplayAbilitySpec AbilitySpec(AbilityClass, 1);
	if (const UDRGameplayAbility* DRAbility = Cast<UDRGameplayAbility>(AbilitySpec.Ability))
	{
		AbilitySpec.DynamicAbilityTags.AddTag(DRAbility->StartupInputTag);
		FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(AbilitySpec);

		// InputTag 캐시에 추가
		if (DRAbility->StartupInputTag.IsValid())
		{
			ASC->AddToInputTagCache(AbilitySpec);
		}
	}

	// BroadcastAbilityInfo가 일찍 return하지 않도록 플래그 설정
	ASC->bStartupAbilitiesGiven = true;

	// UI 아이콘 갱신
	ASC->AbilitiesGivenDelegate.Broadcast();

	if (UOverlayWidgetController* WC = GetOverlayWidgetController())
	{
		WC->BroadcastAbilityInfo();
	}
}

// ========== SeedCannon 적 스폰/정리 ==========

void ADRTutorialManager::SpawnSeedCannonDummies()
{
	if (!DummyEnemyClass) return;

	for (AActor* SpawnPoint : SeedCannonSpawnPoints)
	{
		if (!SpawnPoint) continue;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ADREnemy* NewDummy = GetWorld()->SpawnActor<ADREnemy>(
			DummyEnemyClass,
			SpawnPoint->GetActorLocation(),
			SpawnPoint->GetActorRotation(),
			SpawnParams
		);

		if (NewDummy)
		{
			SetupDummyEnemy(NewDummy);
			SpawnedSeedCannonDummies.Add(NewDummy);
		}
	}
}

void ADRTutorialManager::CleanupSeedCannonDummies()
{
	for (ADREnemy* Dummy : SpawnedSeedCannonDummies)
	{
		if (IsValid(Dummy))
		{
			Dummy->Destroy();
		}
	}
	SpawnedSeedCannonDummies.Empty();
}

// ========== 적 설정 ==========

void ADRTutorialManager::SetupDummyEnemy(ADREnemy* Enemy)
{
	if (!Enemy) return;

	Enemy->bIsTutorialDummy = true;
	Enemy->TutorialManagerRef = this;
}

void ADRTutorialManager::StopPartEnemyAI()
{
	for (ADREnemy* Enemy : PartEnemies)
	{
		if (!Enemy) continue;

		if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
		{
			if (UBrainComponent* Brain = AIC->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Tutorial: Waiting for StartTile"));
			}
		}
	}
}

void ADRTutorialManager::StartPartEnemyAI()
{
	for (ADREnemy* Enemy : PartEnemies)
	{
		if (!Enemy) continue;

		if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
		{
			if (UBrainComponent* Brain = AIC->GetBrainComponent())
			{
				Brain->RestartLogic();
			}
		}
	}
}

// ========== UI ==========

void ADRTutorialManager::UpdateObjectiveUI(const FText& Title, const FText& ProgressFormat, int32 Current, int32 Max)
{
	UOverlayWidgetController* WC = GetOverlayWidgetController();
	if (!WC) return;

	// 스테이지와 동일한 형식: Title + ProgressFormat(라벨), 숫자는 별도 전달
	WC->OnObjectiveTextChanged.Broadcast(Title, ProgressFormat);
	WC->OnObjectiveProgressChanged.Broadcast(Current, Max);
}

UOverlayWidgetController* ADRTutorialManager::GetOverlayWidgetController() const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return nullptr;

	ADRHUD* HUD = Cast<ADRHUD>(PC->GetHUD());
	if (!HUD) return nullptr;

	// WidgetController가 이미 생성되어 있어야 한다 (InitOverlay 이후)
	FWidgetControllerParams Params(PC, PC->PlayerState, nullptr, nullptr);
	return HUD->GetOverlayWidgetController(Params);
}

UDRAbilitySystemComponent* ADRTutorialManager::GetPlayerASC() const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return nullptr;

	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return nullptr;

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Pawn);
	if (!ASI) return nullptr;

	return Cast<UDRAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
}
