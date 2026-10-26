// Copyright DaeRune


#include "UI/WidgetController/OverlayWidgetController.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/Abilities/DRVendingMachineAttackSpeedBuff.h"
#include "GameplayEffect.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "AbilitySystem/Data/StatusEffectInfo.h"
#include "Game/DRChipCatalog.h"
#include "Game/DRGameInstance.h"
#include "Game/DRStageGameState.h"
#include "GameFramework/GameStateBase.h"
#include "Phase/DRPhaseBase.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "UI/Widget/DRUserWidget.h"

#define LOCTEXT_NAMESPACE "DROverlay"

void UOverlayWidgetController::BroadcastInitialValues()
{
	// 게임 시작 시 현재 어트리뷰트 값들을 UI에 전송
	OnHealthChanged.Broadcast(GetDRAS()->GetHealth());
	OnMaxHealthChanged.Broadcast(GetDRAS()->GetMaxHealth());
	OnWaterChanged.Broadcast(GetDRAS()->GetWater());
	OnMaxWaterChanged.Broadcast(GetDRAS()->GetMaxWater());

	// 페이즈 목표 초기값 추가
	HandlePhaseObjectiveChanged();

	// 웨이브 방어 UI 초기 표시 상태 (GameState 복제 플래그 기준 - Plan6 §5.4)
	if (ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>())
	{
		OnPhase3TimerVisibilityChanged.Broadcast(DRGameState->IsWaveDefenseUIActive());
	}
	else
	{
		OnPhase3TimerVisibilityChanged.Broadcast(false);
	}

	// 조준선 초기 상태 (현재 Carrying 태그 기준으로 강제 브로드캐스트)
	UpdateCrosshairState(true);

	// 업그레이드 칩에 따른 HUD 모듈 표시 여부 (Plan3(공용칩) §7.2)
	RefreshHudFeatureVisibility();
}

bool UOverlayWidgetController::IsWaterTankExpanded()
{
	const ADRPlayerState* DRPS = GetDRPS();
	if (!DRPS) return false;

	// FDRResolvedStat::Mult 는 배율(기본 1.0) — Flat 은 기본 0
	const FDRResolvedStat WaterStat = DRPS->GetUpgradeRuntime().GetGlobalStat(EDRUpgradeStat::MaxWater);
	return WaterStat.Flat > KINDA_SMALL_NUMBER || WaterStat.Mult > 1.f + KINDA_SMALL_NUMBER;
}

void UOverlayWidgetController::RefreshHudFeatureVisibility()
{
	OnWaterTankExpandedChanged.Broadcast(IsWaterTankExpanded());

	const ADRPlayerState* DRPS = GetDRPS();

	// ★PlayerState 가 아직 없으면 "보이는" 쪽으로 간다.★
	// 실패했을 때 UI 가 사라지는 것보다 남아 있는 편이 훨씬 안전하다
	// (사라지면 원인을 화면에서 볼 수 없어 디버깅이 어렵다).
	if (!DRPS)
	{
		OnHudFeatureVisibilityChanged.Broadcast(true, true);
		return;
	}

	const UDRGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDRGameInstance>() : nullptr;
	const UDRChipCatalog* Catalog = GI ? GI->GetChipCatalog() : nullptr;

	/**
	 * ★"그 기능을 가두는 칩이 카탈로그에 있을 때만 잠근다."★
	 *
	 * 코드가 에셋보다 먼저 들어가는 것이 정상 순서다(C++ → DA_ChipCatalog 작성).
	 * 이 가드가 없으면 그 사이 기간 동안 모든 플레이어의 HUD 가 통째로 사라진다.
	 * 카탈로그에서 칩을 지웠을 때도 같은 방식으로 자동 복구된다. (Plan3(공용칩) §7.2)
	 */
	auto ShouldShow = [Catalog, DRPS](const FGameplayTag& FeatureTag) -> bool
	{
		if (!Catalog || !Catalog->IsFeatureGated(FeatureTag)) return true;
		return DRPS->HasUpgradeFeature(FeatureTag);
	};

	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
	OnHudFeatureVisibilityChanged.Broadcast(
		ShouldShow(GameplayTags.Upgrade_Feature_HUD_Vitals),
		ShouldShow(GameplayTags.Upgrade_Feature_HUD_SkillIcons));
}

void UOverlayWidgetController::BindCallbacksToDependencies()
{
	UDRAttributeSet* DRAS = GetDRAS();
	if (!AbilitySystemComponent || !DRAS) return;

	// ★모든 바인딩은 this 를 주인으로 묶는다(AddWeakLambda · AddUObject · AddDynamic)★ —
	// UnbindAllDelegates 가 RemoveAll(this) 로 전부 떼어 낼 수 있어야 관전 대상을 바꿀 때
	// 옛 대상의 체력 · 물 · 버프가 HUD 에 섞여 들어오지 않는다.

	// ===== 어트리뷰트 =====
	auto BindAttribute = [this](const FGameplayAttribute& Attribute, FOnAttributeChangedSignature& Signal)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute).AddWeakLambda(
			this, [&Signal](const FOnAttributeChangeData& Data) { Signal.Broadcast(Data.NewValue); });
		BoundAttributes.Add(Attribute);
	};
	BindAttribute(DRAS->GetHealthAttribute(), OnHealthChanged);
	BindAttribute(DRAS->GetMaxHealthAttribute(), OnMaxHealthChanged);
	BindAttribute(DRAS->GetWaterAttribute(), OnWaterChanged);
	BindAttribute(DRAS->GetMaxWaterAttribute(), OnMaxWaterChanged);

	// ===== State.Carrying · 공속 버프 태그 =====
	// State.Carrying 태그 변화 → 조준선 상태 재계산 (서버는 SetCarryingState, 클라는 OnRep 의 Loose 태그 토글로 발화)
	AbilitySystemComponent->RegisterGameplayTagEvent(FDRGameplayTags::Get().State_Carrying, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UOverlayWidgetController::HandleCarryingTagChanged);

	// ★Plan5(공속칩)★ 자판기 공속 버프(자기 버프 + 칩 ② 공유 버프) 스택 → 스킬 아이콘 숫자.
	// AnyCountChange 는 GE 가 붙고 떨어질 때뿐 아니라 ★스택 수만 바뀔 때도★ 서버·클라 모두에서 온다 (D-75)
	AbilitySystemComponent->RegisterGameplayTagEvent(FDRGameplayTags::Get().Buff_VendingMachine_AttackSpeed, EGameplayTagEventType::AnyCountChange)
		.AddUObject(this, &UOverlayWidgetController::HandleAttackSpeedBuffTagChanged);
	AbilitySystemComponent->RegisterGameplayTagEvent(FDRGameplayTags::Get().Buff_AttackSpeed_Shared, EGameplayTagEventType::AnyCountChange)
		.AddUObject(this, &UOverlayWidgetController::HandleAttackSpeedBuffTagChanged);

	if (UDRAbilitySystemComponent* DRASC = GetDRASC())
	{
		// 자판기 잭팟 스택 변경
		DRASC->OnVendingMachineStacksChanged.AddWeakLambda(this, [this](int32 CurrentStacks, int32 MaxStacks)
		{
			OnVendingMachineStackCountChanged.Broadcast(CurrentStacks, MaxStacks);
		});

		// 로봇 청소기 일반 공격(공기탄) 충전 게이지 (3칸)
		DRASC->OnVacuumAirShotGaugeChanged.AddWeakLambda(this, [this](int32 CurrentGauge, int32 MaxGauge)
		{
			OnRobotVacuumAirShotGaugeChanged.Broadcast(CurrentGauge, MaxGauge);
		});

		// 로봇 청소기 돌진 게이지 (5칸)
		DRASC->OnVacuumDashGaugeChanged.AddWeakLambda(this, [this](int32 CurrentGauge, int32 MaxGauge)
		{
			OnRobotVacuumDashGaugeChanged.Broadcast(CurrentGauge, MaxGauge);
		});

		// 버프 · 디버프 아이콘 — 자판기 공격속도 버프 스택은 여기서 방송하지 않는다 (HandleAttackSpeedBuffTagChanged 담당, Plan5(공속칩) D-75)
		DRASC->EffectAssetTags.AddWeakLambda(this,
			[this](const FGameplayTagContainer& AssetTags, bool bHasDuration, const float Duration, bool bDisplayStackCount, const int32 StackCount)
		{
			if (!StatusEffectData) return;

			for (const FGameplayTag& Tag : AssetTags)
			{
				bool bIsDebuff = false;
				if (!IsStatusEffectTag(Tag, bIsDebuff)) continue;

				FEffectInfo EffectInfo = StatusEffectData->FindEffectInfoForTag(Tag);
				if (!EffectInfo.EffectTag.IsValid()) continue;

				EffectInfo.bHasDuration = bHasDuration;
				EffectInfo.Duration = Duration;
				EffectInfo.bDisplayStack = bDisplayStackCount;
				EffectInfo.StackCount = StackCount;
				EffectInfo.bIsDebuff = bIsDebuff;
				StatusEffectWidgetDelegate.Broadcast(EffectInfo);
			}
		});

		// 자판기 공격속도 버프 만료(0) 방송도 HandleAttackSpeedBuffTagChanged 가 맡는다 (Plan5(공속칩) D-75)
		DRASC->EffectRemovedDelegate.AddWeakLambda(this, [this](const FGameplayTagContainer& AssetTags)
		{
			for (const FGameplayTag& Tag : AssetTags)
			{
				bool bIsDebuff = false;
				if (IsStatusEffectTag(Tag, bIsDebuff))
				{
					EffectTagRemovedDelegate.Broadcast(Tag, bIsDebuff);
				}
			}
		});

		// 시작 어빌리티가 이미 부여되었다면 즉시, 아니면 부여 완료 시점에 브로드캐스트
		if (DRASC->bStartupAbilitiesGiven)
		{
			BroadcastAbilityInfo();
		}
		else
		{
			DRASC->AbilitiesGivenDelegate.AddUObject(this, &UOverlayWidgetController::BroadcastAbilityInfo);
		}

		// 스킬 차단 카운터 변화 → UI 재방송
		DRASC->OnBlockedAbilityTagsChanged.AddUObject(this, &UOverlayWidgetController::HandleBlockedTagsChanged);

		// GA 활성화/종료 시 InputTag 기반 Pressed/Released 재방송
		// (PlayerController 직접 broadcast 는 차단 중 게이트로 막히는데, 큐잉되어 나중에 발동된 GA 의 시각 피드백을 여기서 보강)
		DRASC->OnAbilityActivatedWithInputTag.AddWeakLambda(this, [this](const FGameplayTag InputTag)
		{
			OnAbilityInputPressed.Broadcast(InputTag);
		});
		DRASC->OnAbilityEndedWithInputTag.AddWeakLambda(this, [this](const FGameplayTag InputTag)
		{
			// 키를 아직 누르고 있으면 여기서 Released 를 방송하지 않는다 —
			// 즉발 어빌리티(GA_VacuumJetJump)는 활성화와 같은 프레임에 EndAbility 하므로
			// 그대로 방송하면 홀드 중인데도 눌림 이미지가 즉시 풀려버린다.
			// 이 경우 Released 는 실제 키를 뗄 때 ADRPlayerController::AbilityInputTagReleased 가 담당한다.
			// (충전형 LMB/RMB 는 키를 뗄 때 GA 가 끝나므로 이 게이트에 걸리지 않고,
			//  큐잉되어 늦게 발동한 GA 도 그 시점엔 키가 이미 떨어져 있어 기존대로 보강된다)
			if (const ADRPlayerController* DRPC = GetDRPC())
			{
				if (DRPC->IsInputTagHeld(InputTag)) return;
			}

			OnAbilityInputReleased.Broadcast(InputTag);
		});
	}

	// 장착 칩 변경 추종 (HUD 표시 칩) — Plan3(공용칩) §7.2
	if (ADRPlayerState* DRPS = GetDRPS())
	{
		DRPS->OnUpgradeRuntimeChanged.AddUniqueDynamic(
			this, &UOverlayWidgetController::RefreshHudFeatureVisibility);

		// 오염 진입/해제 시 체력 HUD 다시 칠하기 (최대 체력이 그대로인 경우 대비)
		DRPS->OnCorruptedStateChanged.AddUniqueDynamic(
			this, &UOverlayWidgetController::HandleCorruptedStateChanged);
	}

	// GameState 델리게이트 (목표 · 페이즈 알람 · 독가스 경고)
	TryBindGameStateDelegates();
}

bool UOverlayWidgetController::IsStatusEffectTag(const FGameplayTag& Tag, bool& bOutIsDebuff)
{
	static const FGameplayTag BuffRoot = FGameplayTag::RequestGameplayTag(TEXT("Buff"));
	static const FGameplayTag DebuffRoot = FGameplayTag::RequestGameplayTag(TEXT("Debuff"));

	bOutIsDebuff = Tag.MatchesTag(DebuffRoot);
	return bOutIsDebuff || Tag.MatchesTag(BuffRoot);
}

void UOverlayWidgetController::TryBindGameStateDelegates()
{
	if (bGameStateDelegatesBound) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// ★GameState 가 아직 복제되지 않았으면 잠시 뒤 다시 시도한다★ — 한 번만 시도하면 느린 클라에서
	// 목표 · 알람 · 웨이브 타이머 UI 가 영영 묶이지 않는다.
	AGameStateBase* GameStateBase = World->GetGameState();
	if (!GameStateBase)
	{
		World->GetTimerManager().SetTimer(GameStateBindRetryTimer,
			FTimerDelegate::CreateUObject(this, &UOverlayWidgetController::TryBindGameStateDelegates),
			GameStateBindRetryInterval, false);
		return;
	}

	// 스테이지가 아닌 맵(로비 · 튜토리얼)에는 묶을 것이 없다
	if (!GameStateBase->IsA<ADRStageGameState>()) return;

	bGameStateDelegatesBound = true;
	BindPhaseObjectiveDelegate();
	BindPhaseAlarmDelegate();
	BindToxicGasWarningDelegate();
}

void UOverlayWidgetController::HandleCorruptedStateChanged(bool bIsCorrupted)
{
	if (UDRAttributeSet* DRAS = GetDRAS())
	{
		OnMaxHealthChanged.Broadcast(DRAS->GetMaxHealth());
		OnHealthChanged.Broadcast(DRAS->GetHealth());
	}
}

void UOverlayWidgetController::UnbindAllDelegates()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GameStateBindRetryTimer);
	}

	// ===== ASC =====
	if (IsValid(AbilitySystemComponent))
	{
		for (const FGameplayAttribute& Attribute : BoundAttributes)
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute).RemoveAll(this);
		}

		const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
		AbilitySystemComponent->RegisterGameplayTagEvent(GameplayTags.State_Carrying, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		AbilitySystemComponent->RegisterGameplayTagEvent(GameplayTags.Buff_VendingMachine_AttackSpeed, EGameplayTagEventType::AnyCountChange).RemoveAll(this);
		AbilitySystemComponent->RegisterGameplayTagEvent(GameplayTags.Buff_AttackSpeed_Shared, EGameplayTagEventType::AnyCountChange).RemoveAll(this);

		if (UDRAbilitySystemComponent* DRASC = GetDRASC())
		{
			DRASC->OnVendingMachineStacksChanged.RemoveAll(this);
			DRASC->OnVacuumAirShotGaugeChanged.RemoveAll(this);
			DRASC->OnVacuumDashGaugeChanged.RemoveAll(this);
			DRASC->EffectAssetTags.RemoveAll(this);
			DRASC->EffectRemovedDelegate.RemoveAll(this);
			DRASC->AbilitiesGivenDelegate.RemoveAll(this);
			DRASC->OnBlockedAbilityTagsChanged.RemoveAll(this);
			DRASC->OnAbilityActivatedWithInputTag.RemoveAll(this);
			DRASC->OnAbilityEndedWithInputTag.RemoveAll(this);
		}
	}
	BoundAttributes.Reset();

	// ===== PlayerState =====
	if (ADRPlayerState* DRPS = GetDRPS())
	{
		DRPS->OnUpgradeRuntimeChanged.RemoveDynamic(
			this, &UOverlayWidgetController::RefreshHudFeatureVisibility);
		DRPS->OnCorruptedStateChanged.RemoveDynamic(
			this, &UOverlayWidgetController::HandleCorruptedStateChanged);
	}

	// ===== GameState =====
	bGameStateDelegatesBound = false;
	ADRStageGameState* DRGameState = GetWorld() ? GetWorld()->GetGameState<ADRStageGameState>() : nullptr;
	if (!DRGameState) return;

	DRGameState->OnPhaseObjectiveChangedDelegate.RemoveAll(this);
	DRGameState->OnObjectiveCompletedDelegate.RemoveAll(this);
	DRGameState->OnWaveTimerChangedDelegate.RemoveAll(this);
	DRGameState->OnPhaseChangedDelegate.RemoveDynamic(this, &UOverlayWidgetController::OnPhaseChanged);
	DRGameState->OnWaveDefenseUIActiveChangedDelegate.RemoveDynamic(this, &UOverlayWidgetController::HandleWaveDefenseUIActiveChanged);
	DRGameState->OnToxicGasWarningDelegate.RemoveDynamic(this, &UOverlayWidgetController::OnToxicGasWarningReceived);
}

void UOverlayWidgetController::HandlePhaseObjectiveChanged()
{
	const ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
    if (!DRGameState) return;

    const FPhaseObjectiveData ObjectiveData = DRGameState->GetCurrentPhaseObjective();
    int32 CurrentProgress = DRGameState->GetCurrentObjectiveProgress();

	// PhaseNumber 0 = 실제 목표가 설정되기 전 준비 상태 (생성자 기본값 "Preparing...")
	// 위젯이 placeholder를 첫 목표로 오인해 진짜 첫 목표를 Pending으로 잡아버리는 것을 방지
	if (ObjectiveData.PhaseNumber == 0) return;

	OnObjectiveTextChanged.Broadcast(ObjectiveData.ObjectiveTitle,ObjectiveData.ProgressFormat);
	OnObjectiveProgressChanged.Broadcast(CurrentProgress,ObjectiveData.RequiredCount);
	OnObjectiveProgressVisibilityChanged.Broadcast(ObjectiveData.RequiredCount > 0);

	// ===== 페이즈 알람 배너 (Plan6 §5.4) =====
	// 목표 데이터의 PhaseAlarmText를 우선 사용하고, 비어 있으면 레거시 문구로 폴백한다.
	// 알람은 목표가 도착한 이 시점에 띄운다 (페이즈 인덱스 복제가 목표보다 먼저 도착하기 때문).
	if (bPhaseAlarmPending)
	{
		FText AlarmText = ObjectiveData.PhaseAlarmText;
		if (AlarmText.IsEmpty())
		{
			AlarmText = GetLegacyPhaseAlarmText(CachedPhaseNumber);
		}

		if (!AlarmText.IsEmpty())
		{
			OnPhaseAlarm.Broadcast(AlarmText);
			bPhaseAlarmShown = true;
			CachedAlarmText = AlarmText;
		}
		bPhaseAlarmPending = false;
	}
	else if (!ObjectiveData.PhaseAlarmText.IsEmpty()
		&& !ObjectiveData.PhaseAlarmText.EqualTo(CachedAlarmText))
	{
		// 같은 페이즈 안에서 목표가 교체되는 경우(스테이지2 서브 목표):
		// PhaseAlarmText가 채워진 행에서만 배너를 다시 띄운다. 빈 행은 배너 없이 목표만 교체된다.
		OnPhaseAlarm.Broadcast(ObjectiveData.PhaseAlarmText);
		bPhaseAlarmShown = true;
		CachedAlarmText = ObjectiveData.PhaseAlarmText;
	}
}

void UOverlayWidgetController::BindPhaseObjectiveDelegate()
{
	ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
	if (!DRGameState) return;

	DRGameState->OnPhaseObjectiveChangedDelegate.AddWeakLambda(this, [this]()
	{
		HandlePhaseObjectiveChanged();
		CheckAndBindWaveTimer();
	});

	// 목표 클리어 델리게이트 바인딩 (Multicast RPC → 클리어 애니메이션 트리거)
	DRGameState->OnObjectiveCompletedDelegate.AddWeakLambda(this, [this]()
	{
		OnObjectiveCompleted.Broadcast();
	});

	// 현재 값 즉시 받아오기
	HandlePhaseObjectiveChanged();
}

void UOverlayWidgetController::BindWaveTimerDelegate()
{
	ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
	if (!DRGameState) return;

	// 웨이브 방어 UI 가 꺼졌다 켜지면 다시 불린다 — 먼저 떼어 중복 바인딩을 막는다
	DRGameState->OnWaveTimerChangedDelegate.RemoveAll(this);
	DRGameState->OnWaveTimerChangedDelegate.AddWeakLambda(this,
		[this](int32 WaveNumber, float RemainingTime, bool bIsRestTime)
	{
		OnWaveTimerChanged.Broadcast(WaveNumber, RemainingTime, bIsRestTime);
	});

	// 초기값 즉시 브로드캐스트
	OnWaveTimerChanged.Broadcast(
		DRGameState->GetCurrentWaveNumber(),
		DRGameState->GetWaveRemainingTime(),
		DRGameState->IsWaveRestTime()
	);
}

void UOverlayWidgetController::BindPhaseAlarmDelegate()
{
	ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
	if (!DRGameState) return;

	// 싱글톤 WidgetController이므로 이전 게임의 상태를 초기화
	CachedPhaseNumber = -1;
	CachedWavePhaseNumber = -1;
	bPhaseAlarmShown = false;
	bPhaseAlarmPending = false;
	CachedAlarmText = FText::GetEmpty();

	DRGameState->OnPhaseChangedDelegate.AddUniqueDynamic(this, &UOverlayWidgetController::OnPhaseChanged);

	// 웨이브 방어 UI 표시 플래그 구독 (Plan6 §5.4)
	DRGameState->OnWaveDefenseUIActiveChangedDelegate.AddUniqueDynamic(
		this, &UOverlayWidgetController::HandleWaveDefenseUIActiveChanged);

	// 현재 페이즈 즉시 표시
	int32 CurrentPhase = DRGameState->GetCurrentPhaseIndex();
	if (CurrentPhase >= 0)
	{
		OnPhaseChanged(CurrentPhase);

		// 바인딩 시점에 이미 목표가 설정되어 있다면(늦은 바인딩/재접속) 대기 중인 알람을 즉시 해소한다.
		// 목표가 아직 없으면 PhaseNumber == 0 가드에 걸려 아무 일도 하지 않고,
		// 이후 목표가 도착할 때 정상적으로 알람이 표시된다.
		HandlePhaseObjectiveChanged();
	}

	// 늦게 접속한 클라이언트 대응: 이미 웨이브 방어 페이즈가 진행 중일 수 있다
	if (DRGameState->IsWaveDefenseUIActive())
	{
		HandleWaveDefenseUIActiveChanged(true);
	}
}

void UOverlayWidgetController::BindToxicGasWarningDelegate()
{
	ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
	if (!DRGameState) return;

	DRGameState->OnToxicGasWarningDelegate.AddUniqueDynamic(this, &UOverlayWidgetController::OnToxicGasWarningReceived);

	// 현재 상태 즉시 확인 (이미 독가스 웨이브 진행 중일 수 있음 - late joiner)
	if (DRGameState->IsToxicGasWave())
	{
		OnToxicGasWarning.Broadcast(true);
	}
}

void UOverlayWidgetController::OnToxicGasWarningReceived(bool bIsToxicGasWave)
{
	OnToxicGasWarning.Broadcast(bIsToxicGasWave);
}

void UOverlayWidgetController::CheckAndBindWaveTimer()
{
	ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
	if (!DRGameState) return;

	// 웨이브 방어 UI를 쓰는 페이즈일 때만 바인딩 (Plan6 §5.4 - 페이즈 인덱스 하드코딩 제거)
	// CachedPhaseNumber가 아닌 별도의 CachedWavePhaseNumber 사용 (Phase 알람과 분리)
	if (DRGameState->IsWaveDefenseUIActive())
	{
		// 이미 바인딩되었는지 체크
		if (CachedWavePhaseNumber != 2)
		{
			CachedWavePhaseNumber = 2;
			BindWaveTimerDelegate();
		}
	}
	else
	{
		// 웨이브 방어 페이즈가 아니면 캐시 초기화
		if (CachedWavePhaseNumber == 2)
		{
			CachedWavePhaseNumber = -1;
		}
	}
}

void UOverlayWidgetController::HandleWaveDefenseUIActiveChanged(bool bIsActive)
{
	// 웨이브 타이머 UI 표시/숨김
	OnPhase3TimerVisibilityChanged.Broadcast(bIsActive);

	if (bIsActive)
	{
		if (CachedWavePhaseNumber != 2)
		{
			CachedWavePhaseNumber = 2;
			BindWaveTimerDelegate();
		}
	}
	else
	{
		CachedWavePhaseNumber = -1;
	}
}

void UOverlayWidgetController::OnPhaseChanged(int32 NewPhaseIndex)
{
	// Plan6 §5.4: 웨이브 방어 UI(타이머 + 클렌저 HP)는 페이즈 인덱스가 아니라
	// GameState의 bWaveDefenseUIActive 플래그가 담당한다 (HandleWaveDefenseUIActiveChanged).

	// 알람 문구는 여기서 결정하지 않는다.
	// GameMode가 페이즈 인덱스를 먼저 복제한 뒤 페이즈의 OnPhaseStart가 목표를 설정하므로,
	// 이 시점의 CurrentPhaseObjective는 아직 이전 페이즈의 데이터다.
	// 실제 브로드캐스트는 목표 데이터가 도착한 HandlePhaseObjectiveChanged에서 수행한다.
	if (CachedPhaseNumber != NewPhaseIndex)
	{
		CachedPhaseNumber = NewPhaseIndex;
		bPhaseAlarmPending = true;
	}
}

FText UOverlayWidgetController::GetLegacyPhaseAlarmText(int32 PhaseIndex) const
{
	// 목표 DataTable에 PhaseAlarmText가 채워지기 전까지 사용하는 폴백 (스테이지1 호환)
	switch (PhaseIndex)
	{
	case 0:
		return LOCTEXT("Phase_Alarm_1", "페이즈 1: 확보");
	case 1:
		return LOCTEXT("Phase_Alarm_2", "페이즈 2: 수집");
	case 2:
		return LOCTEXT("Phase_Alarm_3", "페이즈 3: 방어");
	default:
		return FText::GetEmpty();
	}
}

void UOverlayWidgetController::HandleBlockedTagsChanged()
{
	OnAbilityBlockStateDirty.Broadcast();
}

bool UOverlayWidgetController::IsAbilityBlockedNow(FGameplayTag AbilityTag) const
{
	if (!AbilitySystemComponent) return false;

	// Carrying 중에는 어떤 스킬도 못 쓰므로 모든 슬롯 차단으로 간주
	if (AbilitySystemComponent->HasMatchingGameplayTag(FDRGameplayTags::Get().State_Carrying))
	{
		return true;
	}

	if (!AbilityTag.IsValid()) return false;

	FGameplayTagContainer Single;
	Single.AddTag(AbilityTag);
	return AbilitySystemComponent->AreAbilityTagsBlocked(Single);
}

void UOverlayWidgetController::BroadcastSkillIconWidgetClass()
{
	ADRPlayerState* PS = GetDRPS();
	if (!PS) return;

	EPlayerCharacterClass CharClass = PS->GetSelectedPlayerClass();

	UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(GetWorld());
	if (!ClassInfo) return;

	const FCharacterClassDefaultInfo& DefaultInfo = ClassInfo->GetClassDefaultInfo(CharClass);
	if (DefaultInfo.SkillIconWidgetClass)
	{
		OnSkillIconClassChanged.Broadcast(DefaultInfo.SkillIconWidgetClass);
	}
}

void UOverlayWidgetController::BroadcastCrosshairImages()
{
	ADRPlayerState* PS = GetDRPS();
	if (!PS) return;

	UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(GetWorld());
	if (!ClassInfo) return;

	const FCharacterClassDefaultInfo& DefaultInfo = ClassInfo->GetClassDefaultInfo(PS->GetSelectedPlayerClass());
	OnCrosshairImagesChanged.Broadcast(
		DefaultInfo.CrosshairTexture,
		ClassInfo->CrosshairDisabledTexture,
		DefaultInfo.CrosshairInstallReadyTexture,
		DefaultInfo.CrosshairSize);
}

void UOverlayWidgetController::NotifyCrosshairEnemyHit()
{
	OnCrosshairHitConfirmed.Broadcast();
}

void UOverlayWidgetController::SetCrosshairSiteDetected(bool bDetected)
{
	if (bCrosshairSiteDetected == bDetected) return;

	bCrosshairSiteDetected = bDetected;
	UpdateCrosshairState();
}

void UOverlayWidgetController::HandleCarryingTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	// 부품을 내려놓으면(설치/드롭/사망) 사이트 감지 상태도 함께 초기화
	if (NewCount <= 0)
	{
		bCrosshairSiteDetected = false;
	}
	UpdateCrosshairState();
}

void UOverlayWidgetController::HandleAttackSpeedBuffTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	const FDRGameplayTags& Tags = FDRGameplayTags::Get();

	// NewCount 는 스택 수가 아니라 "그 태그를 주는 GE 개수"다. 0 이면 방금 완전히 떨어진 것이라
	// 제거 대기 중인 GE 를 다시 세지 않도록 그 태그 몫은 0 으로 본다.
	auto StacksOf = [this, &CallbackTag, NewCount](const FGameplayTag& Tag) -> int32
	{
		if (Tag == CallbackTag && NewCount <= 0) return 0;
		return UDRAbilitySystemLibrary::GetEffectStackCountWithTag(AbilitySystemComponent, Tag);
	};

	const int32 SelfStacks = StacksOf(Tags.Buff_VendingMachine_AttackSpeed);
	const int32 SharedStacks = StacksOf(Tags.Buff_AttackSpeed_Shared);

	// ★방송보다 먼저★ 감소 구간 여부를 갱신한다 — 위젯이 이 방송을 받자마자 GetAttackSpeedBuffFillRatio 를 부른다.
	// 자기 버프 스택이 줄어드는 길은 칩 ③의 1초 감소뿐이다(나머지는 한꺼번에 0 이 된다) (T-73)
	if (SelfStacks <= 0 || SelfStacks > LastSelfAttackSpeedStacks)
	{
		bAttackSpeedBuffDecaying = false;
	}
	else if (SelfStacks < LastSelfAttackSpeedStacks)
	{
		bAttackSpeedBuffDecaying = true;
	}
	LastSelfAttackSpeedStacks = SelfStacks;

	// 칩 ②를 낀 자판기는 자기 버프 대신 공유 버프를 받으므로 둘을 합쳐 보여 준다 (D-70)
	OnVendingMachineAttackSpeedBuff.Broadcast(SelfStacks + SharedStacks);
}

float UOverlayWidgetController::GetAttackSpeedBuffFillRatio() const
{
	const UAbilitySystemComponent* ASC = AbilitySystemComponent;
	const UWorld* World = GetWorld();
	if (!ASC || !World) return 0.f;

	const FDRGameplayTags& Tags = FDRGameplayTags::Get();
	const float Now = World->GetTimeSeconds();

	// 태그를 주는 첫 활성 GE 의 채움 비율. Aggregate by Target 이라 인스턴스는 하나다.
	auto RatioOf = [this, ASC, Now](const FGameplayTag& Tag) -> float
	{
		FGameplayTagContainer Filter;
		Filter.AddTag(Tag);

		for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffectsWithAllTags(Filter))
		{
			const FActiveGameplayEffect* ActiveGE = ASC->GetActiveGameplayEffect(Handle);
			if (!ActiveGE || !ActiveGE->Spec.Def) continue;

			// 남은 시간 — 시작 시각은 서버가 복제하므로 클라에서도 정확하다
			const float Remaining = ActiveGE->GetTimeRemaining(Now);
			float Total = ActiveGE->GetDuration();   // 기본 20 / 칩 ① 5 / 칩 ② 20 (코드가 스펙에 고정해 넣은 값)

			// 칩 ③ GE — 만료 때 1스택씩 빼는 정책. Spec 지속은 감소 간격(1초)이고 첫 만료만 BuffDuration 뒤로 밀려 있다 (D-73)
			if (ActiveGE->Spec.Def->GetStackExpirationPolicy() == EGameplayEffectStackingExpirationPolicy::RemoveSingleStackAndRefreshDuration)
			{
				// A 방식: 1초씩 줄어드는 구간에는 그래프를 비우고 숫자만 줄인다
				if (bAttackSpeedBuffDecaying) return 0.f;
				Total = UDRVendingMachineAttackSpeedBuff::FindBuffDuration(ASC);
			}

			return Total > 0.f ? FMath::Clamp(Remaining / Total, 0.f, 1.f) : 0.f;
		}
		return 0.f;
	};

	// 자기 버프와 공유 버프(다른 자판기의 칩 ②)가 같이 있으면 더 많이 남은 쪽
	return FMath::Max(RatioOf(Tags.Buff_VendingMachine_AttackSpeed), RatioOf(Tags.Buff_AttackSpeed_Shared));
}

void UOverlayWidgetController::UpdateCrosshairState(bool bForceBroadcast)
{
	EDRCrosshairState NewState = EDRCrosshairState::Normal;
	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(FDRGameplayTags::Get().State_Carrying))
	{
		NewState = bCrosshairSiteDetected ? EDRCrosshairState::InstallReady : EDRCrosshairState::Disabled;
	}

	if (!bForceBroadcast && NewState == CachedCrosshairState) return;

	CachedCrosshairState = NewState;
	OnCrosshairStateChanged.Broadcast(NewState);
}

#undef LOCTEXT_NAMESPACE

