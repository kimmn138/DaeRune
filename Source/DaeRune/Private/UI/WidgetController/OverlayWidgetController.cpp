// Copyright DaeRune


#include "UI/WidgetController/OverlayWidgetController.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/DRCleanserSiteAttributeSet.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "AbilitySystem/Data/StatusEffectInfo.h"
#include "Actor/DRCleanserSite.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Phase/DRPhase3.h"
#include "Phase/DRPhaseBase.h"
#include "Player/DRPlayerState.h"
#include "UI/Widget/DRUserWidget.h"

void UOverlayWidgetController::BroadcastInitialValues()
{
	// ���� ���� �� ���� ��Ʈ����Ʈ ������ UI�� ����
	OnHealthChanged.Broadcast(GetDRAS()->GetHealth());
	OnMaxHealthChanged.Broadcast(GetDRAS()->GetMaxHealth());
	OnWaterChanged.Broadcast(GetDRAS()->GetWater());
	OnMaxWaterChanged.Broadcast(GetDRAS()->GetMaxWater());

	// 페이즈 목표 초기값 추가
	HandlePhaseObjectiveChanged();

	// Phase3 타이머 UI 초기 표시 상태 (현재 페이즈 기준)
	if (ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>())
	{
		OnPhase3TimerVisibilityChanged.Broadcast(DRGameState->GetCurrentPhaseIndex() == 2);
	}
	else
	{
		OnPhase3TimerVisibilityChanged.Broadcast(false);
	}

	// 조준선 초기 상태 (현재 Carrying 태그 기준으로 강제 브로드캐스트)
	UpdateCrosshairState(true);
}

void UOverlayWidgetController::BindCallbacksToDependencies()
{
	// 약 참조 캡처로 안전한 람다 바인딩
	TWeakObjectPtr<UOverlayWidgetController> WeakThis(this);

	// �ݹ� ���ε�
	FDelegateHandle HealthHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GetDRAS()->GetHealthAttribute()).AddLambda(
		[WeakThis](const FOnAttributeChangeData& Data)
		{
			if (UOverlayWidgetController* StrongThis = WeakThis.Get())
			{
				StrongThis->OnHealthChanged.Broadcast(Data.NewValue);
			}
		}
	);
	ASCDelegateHandles.Add(HealthHandle);

	FDelegateHandle MaxHealthHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GetDRAS()->GetMaxHealthAttribute()).AddLambda(
		[WeakThis](const FOnAttributeChangeData& Data)
		{
			if (UOverlayWidgetController* StrongThis = WeakThis.Get())
			{
				StrongThis->OnMaxHealthChanged.Broadcast(Data.NewValue);
			}
		}
	);
	ASCDelegateHandles.Add(MaxHealthHandle);

	FDelegateHandle WaterHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GetDRAS()->GetWaterAttribute()).AddLambda(
		[WeakThis](const FOnAttributeChangeData& Data)
		{
			if (UOverlayWidgetController* StrongThis = WeakThis.Get())
			{
				StrongThis->OnWaterChanged.Broadcast(Data.NewValue);
			}
		}
	);
	ASCDelegateHandles.Add(WaterHandle);

	FDelegateHandle MaxWaterHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GetDRAS()->GetMaxWaterAttribute()).AddLambda(
		[WeakThis](const FOnAttributeChangeData& Data)
		{
			if (UOverlayWidgetController* StrongThis = WeakThis.Get())
			{
				StrongThis->OnMaxWaterChanged.Broadcast(Data.NewValue);
			}
		}
	);
	ASCDelegateHandles.Add(MaxWaterHandle);
	

	// �����Ƽ ���� �ʱ�ȭ ó��
	if (GetDRASC())
	{
		// 자판기 잭팟 스택 변경 델리게이트 바인딩
		GetDRASC()->OnVendingMachineStacksChanged.AddLambda(
			[WeakThis](int32 CurrentStacks, int32 MaxStacks)
			{
				if (UOverlayWidgetController* StrongThis = WeakThis.Get())
				{
					StrongThis->OnVendingMachineStackCountChanged.Broadcast(CurrentStacks, MaxStacks);
				}
			}
		);

		GetDRASC()->EffectAssetTags.AddLambda(
			[WeakThis](const FGameplayTagContainer& AssetTags, bool HasDuration, const float Duration, bool DisplayStackCount, const int32 StackCount)
			{
				UOverlayWidgetController* StrongThis = WeakThis.Get();
				if (!StrongThis || !StrongThis->StatusEffectData) return;

				const FGameplayTag BuffTag = FGameplayTag::RequestGameplayTag(FName("Buff"));
				const FGameplayTag DebuffTag = FGameplayTag::RequestGameplayTag(FName("Debuff"));
				const FGameplayTag AttackSpeedBuffTag = FDRGameplayTags::Get().Buff_VendingMachine_AttackSpeed;
				for (const FGameplayTag& Tag : AssetTags)
				{
					// 자판기 공격속도 버프 감지 (현재 스택 수 전달)
					if (Tag.MatchesTagExact(AttackSpeedBuffTag))
					{
						StrongThis->OnVendingMachineAttackSpeedBuff.Broadcast(StackCount);
					}

					if (Tag.MatchesTag(BuffTag))
					{
						FEffectInfo EffectInfo = StrongThis->StatusEffectData->FindEffectInfoForTag(Tag);
						if (EffectInfo.EffectTag.IsValid())
						{
							EffectInfo.bHasDuration = HasDuration;
							EffectInfo.Duration = Duration;
							EffectInfo.bDisplayStack = DisplayStackCount;
							EffectInfo.StackCount = StackCount;
							StrongThis->StatusEffectWidgetDelegate.Broadcast(EffectInfo);
						}
					}
					if (Tag.MatchesTag(DebuffTag))
					{
						FEffectInfo EffectInfo = StrongThis->StatusEffectData->FindEffectInfoForTag(Tag);
						if (EffectInfo.EffectTag.IsValid())
						{
							EffectInfo.bHasDuration = HasDuration;
							EffectInfo.Duration = Duration;
							EffectInfo.bDisplayStack = DisplayStackCount;
							EffectInfo.StackCount = StackCount;
							EffectInfo.bIsDebuff = true;
							StrongThis->StatusEffectWidgetDelegate.Broadcast(EffectInfo);
						}
					}
				}
			}
		);

		GetDRASC()->EffectRemovedDelegate.AddLambda(
			[WeakThis](const FGameplayTagContainer& AssetTags)
			{
				UOverlayWidgetController* StrongThis = WeakThis.Get();
				if (!StrongThis) return;

				FGameplayTag BuffTag = FGameplayTag::RequestGameplayTag(FName("Buff"));
				FGameplayTag DebuffTag = FGameplayTag::RequestGameplayTag(FName("Debuff"));
				const FGameplayTag AttackSpeedBuffTag = FDRGameplayTags::Get().Buff_VendingMachine_AttackSpeed;
				for (const FGameplayTag& Tag : AssetTags)
				{
					// 자판기 공격속도 버프 만료 시 스킬 아이콘 스택 0으로 리셋
					if (Tag.MatchesTagExact(AttackSpeedBuffTag))
					{
						StrongThis->OnVendingMachineAttackSpeedBuff.Broadcast(0);
					}
					if (Tag.MatchesTag(BuffTag))
					{
						StrongThis->EffectTagRemovedDelegate.Broadcast(Tag, false);
					}
					if (Tag.MatchesTag(DebuffTag))
					{
						StrongThis->EffectTagRemovedDelegate.Broadcast(Tag, true);
					}
				}
			}
		);
		
		// ���� �����Ƽ�� �̹� �ο��Ǿ��ٸ� ��� ��ε�ĳ��Ʈ
		if (GetDRASC()->bStartupAbilitiesGiven)
		{
			BroadcastAbilityInfo();
		}
		else
		{
			// ���� �ο����� �ʾҴٸ� �ο� �Ϸ� ������ ��ε�ĳ��Ʈ�ϵ��� ��������Ʈ ���ε�
			GetDRASC()->AbilitiesGivenDelegate.AddUObject(this, &UOverlayWidgetController::BroadcastAbilityInfo);
		}

		// 스킬 차단 카운터 변화 → UI 재방송
		GetDRASC()->OnBlockedAbilityTagsChanged.AddUObject(this, &UOverlayWidgetController::HandleBlockedTagsChanged);

		// State.Carrying 태그 변화 → 조준선 상태 재계산 (서버는 SetCarryingState, 클라는 OnRep 의 Loose 태그 토글로 발화)
		AbilitySystemComponent->RegisterGameplayTagEvent(FDRGameplayTags::Get().State_Carrying, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UOverlayWidgetController::HandleCarryingTagChanged);

		// GA 활성화/종료 시 InputTag 기반 Pressed/Released 재방송
		// (PlayerController 직접 broadcast 는 차단 중 게이트로 막히는데, 큐잉되어 나중에 발동된 GA 의 시각 피드백을 여기서 보강)
		GetDRASC()->OnAbilityActivatedWithInputTag.AddLambda(
			[WeakThis](const FGameplayTag InputTag)
			{
				if (UOverlayWidgetController* StrongThis = WeakThis.Get())
				{
					StrongThis->OnAbilityInputPressed.Broadcast(InputTag);
				}
			}
		);
		GetDRASC()->OnAbilityEndedWithInputTag.AddLambda(
			[WeakThis](const FGameplayTag InputTag)
			{
				if (UOverlayWidgetController* StrongThis = WeakThis.Get())
				{
					StrongThis->OnAbilityInputReleased.Broadcast(InputTag);
				}
			}
		);
	}

	// GameState 페이즈 목표 델리게이트 바인딩
	// 딜레이를 최소화하여 Phase 알림을 놓치지 않도록 함
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PhaseBindingDelayTimer,
			this,
			&UOverlayWidgetController::BindPhaseObjectiveDelegate,
			0.1f,  // 0.1초 대기 (GameState 복제 대기)
			false  // 한 번만 실행
		);
	}

	// Phase 변경 델리게이트 바인딩
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PhaseAlarmBindingDelayTimer,
			this,
			&UOverlayWidgetController::BindPhaseAlarmDelegate,
			0.1f,  // 0.1초 대기 (GameState 복제 대기)
			false  // 한 번만 실행
		);
	}

	// 독가스 경고 델리게이트 바인딩
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ToxicGasWarningBindingDelayTimer,
			this,
			&UOverlayWidgetController::BindToxicGasWarningDelegate,
			0.1f,  // 0.1초 대기 (GameState 복제 대기)
			false  // 한 번만 실행
		);
	}
}

void UOverlayWidgetController::BindCallbacksCleanserSiteToDependencies()
{
	// 클렌저사이트 체력 델리게이트 바인딩 (서버 전용)
	if (ADRStageGameMode* SGM = Cast<ADRStageGameMode>(GetWorld()->GetAuthGameMode()))
	{
		if(UDRPhase3* Phase3 = Cast<UDRPhase3>(SGM->GetCurrentPhase()))
		{
			Phase3->OnCleanserSiteReadyDelegate.AddDynamic(this, &UOverlayWidgetController::BindCleanserSite);
		}
	}
}

void UOverlayWidgetController::UnbindAllDelegates()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// ASC 델리게이트 언바인딩
	if (AbilitySystemComponent)
	{
		for (const FDelegateHandle& Handle : ASCDelegateHandles)
		{
			if (Handle.IsValid())
			{
				// ASC의 모든 attribute delegate에서 제거 시도
				// Note: GetGameplayAttributeValueChangeDelegate는 specific attribute가 필요하므로
				// 여기서는 핸들만 무효화 (람다에서 WeakThis 체크로 안전)
			}
		}
		ASCDelegateHandles.Empty();
	}

	// CleanserSite ASC 델리게이트 언바인딩
	for (const auto& Pair : CleanserSiteDelegateHandles)
	{
		if (UAbilitySystemComponent* ASC = Pair.Key.Get())
		{
			// 핸들이 유효하면 언바인딩 (람다에서 WeakThis 체크로 안전)
		}
	}
	CleanserSiteDelegateHandles.Empty();

	ADRStageGameState* DRGameState = World->GetGameState<ADRStageGameState>();
	if (!DRGameState) return;

	// PhaseObjective 델리게이트 언바인딩
	if (PhaseObjectiveDelegateHandle.IsValid())
	{
		DRGameState->OnPhaseObjectiveChangedDelegate.Remove(PhaseObjectiveDelegateHandle);
		PhaseObjectiveDelegateHandle.Reset();
	}

	// ObjectiveCompleted 델리게이트 언바인딩
	if (ObjectiveCompletedDelegateHandle.IsValid())
	{
		DRGameState->OnObjectiveCompletedDelegate.Remove(ObjectiveCompletedDelegateHandle);
		ObjectiveCompletedDelegateHandle.Reset();
	}

	// WaveTimer 델리게이트 언바인딩
	if (WaveTimerDelegateHandle.IsValid())
	{
		DRGameState->OnWaveTimerChangedDelegate.Remove(WaveTimerDelegateHandle);
		WaveTimerDelegateHandle.Reset();
	}

	DRGameState->OnPhaseChangedDelegate.RemoveDynamic(this, &UOverlayWidgetController::OnPhaseChanged);
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
}

void UOverlayWidgetController::BindPhaseObjectiveDelegate()
{
	ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
	if (!DRGameState) return;

	// 약 참조 캡처로 안전한 람다 바인딩
	TWeakObjectPtr<UOverlayWidgetController> WeakThis(this);

	// 핸들 저장하면서 델리게이트 바인딩
	PhaseObjectiveDelegateHandle = DRGameState->OnPhaseObjectiveChangedDelegate.AddLambda(
		[WeakThis]()
		{
			if (UOverlayWidgetController* StrongThis = WeakThis.Get())
			{
				StrongThis->HandlePhaseObjectiveChanged();
				StrongThis->CheckAndBindWaveTimer();
			}
		}
	);

	// 목표 클리어 델리게이트 바인딩 (Multicast RPC → 클리어 애니메이션 트리거)
	ObjectiveCompletedDelegateHandle = DRGameState->OnObjectiveCompletedDelegate.AddLambda(
		[WeakThis]()
		{
			if (UOverlayWidgetController* StrongThis = WeakThis.Get())
			{
				StrongThis->OnObjectiveCompleted.Broadcast();
			}
		}
	);

	// 현재 값 즉시 받아오기
	HandlePhaseObjectiveChanged();
}

void UOverlayWidgetController::BindWaveTimerDelegate()
{
	ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
	if (!DRGameState) return;

	// 약 참조 캡처로 안전한 람다 바인딩
	TWeakObjectPtr<UOverlayWidgetController> WeakThis(this);

	// 웨이브 타이머 델리게이트 바인딩
	WaveTimerDelegateHandle = DRGameState->OnWaveTimerChangedDelegate.AddLambda(
		[WeakThis](int32 WaveNumber, float RemainingTime, bool bIsRestTime)
		{
			if (UOverlayWidgetController* StrongThis = WeakThis.Get())
			{
				StrongThis->OnWaveTimerChanged.Broadcast(WaveNumber, RemainingTime, bIsRestTime);
			}
		}
	);

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

	DRGameState->OnPhaseChangedDelegate.AddDynamic(this, &UOverlayWidgetController::OnPhaseChanged);

	// 현재 페이즈 즉시 표시
	int32 CurrentPhase = DRGameState->GetCurrentPhaseIndex();
	if (CurrentPhase >= 0)
	{
		OnPhaseChanged(CurrentPhase);
	}
}

void UOverlayWidgetController::BindToxicGasWarningDelegate()
{
	ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
	if (!DRGameState) return;

	DRGameState->OnToxicGasWarningDelegate.AddDynamic(this, &UOverlayWidgetController::OnToxicGasWarningReceived);

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

	const int32 CurrentPhase = DRGameState->GetCurrentPhaseIndex();

	// Phase 3일 때만 바인딩 (인덱스 2)
	// CachedPhaseNumber가 아닌 별도의 CachedWavePhaseNumber 사용 (Phase 알람과 분리)
	if (CurrentPhase == 2)
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
		// Phase 3가 아니면 캐시 초기화
		if (CachedWavePhaseNumber == 2)
		{
			CachedWavePhaseNumber = -1;
		}
	}
}

void UOverlayWidgetController::OnPhaseChanged(int32 NewPhaseIndex)
{
	// Phase 3 (인덱스 2)로 변경되었을 때만 처리
	if (NewPhaseIndex == 2)
	{
		BindCallbacksCleanserSiteToDependencies();
	}

	// Phase 3 진입 시점에만 타이머 UI 표시, 그 외엔 숨김
	// 매번 브로드캐스트해도 위젯 측에서 SetVisibility는 idempotent 하므로 안전
	OnPhase3TimerVisibilityChanged.Broadcast(NewPhaseIndex == 2);

	if (CachedPhaseNumber != NewPhaseIndex || !bPhaseAlarmShown)
	{
		FText PhaseText;
		switch (NewPhaseIndex)
		{
		case 0:
			PhaseText = FText::FromString(TEXT("페이즈 1: 확보"));
			break;
		case 1:
			PhaseText = FText::FromString(TEXT("페이즈 2: 수집"));
			break;
		case 2:
			PhaseText = FText::FromString(TEXT("페이즈 3: 방어"));
			break;
		default:
			PhaseText = FText::FromString(TEXT("알 수 없는 페이즈"));
			break;
		}

		// 페이즈가 실제로 변경되었을 때만 알람 브로드캐스트
		if (CachedPhaseNumber != NewPhaseIndex)
		{
			OnPhaseAlarm.Broadcast(PhaseText);
			bPhaseAlarmShown = true;
		}

		CachedPhaseNumber = NewPhaseIndex;
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

	FCharacterClassDefaultInfo DefaultInfo = ClassInfo->GetClassDefaultInfo(CharClass);
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

	const FCharacterClassDefaultInfo DefaultInfo = ClassInfo->GetClassDefaultInfo(PS->GetSelectedPlayerClass());
	OnCrosshairImagesChanged.Broadcast(
		DefaultInfo.CrosshairTexture,
		ClassInfo->CrosshairDisabledTexture,
		DefaultInfo.CrosshairInstallReadyTexture);
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

void UOverlayWidgetController::BindCleanserSite(ADRCleanserSite* FirstCleanserSite,ADRCleanserSite* SecondCleanserSite)
{
	if (!FirstCleanserSite || !SecondCleanserSite) return;

	UAbilitySystemComponent* FirstSiteAsc = FirstCleanserSite->GetAbilitySystemComponent();
	const UDRCleanserSiteAttributeSet* FirstSiteAs = FirstCleanserSite->GetAttributeSet();
	if (!FirstSiteAsc || !FirstSiteAs) return;

	UAbilitySystemComponent* SecondSiteAsc = SecondCleanserSite->GetAbilitySystemComponent();
	const UDRCleanserSiteAttributeSet* SecondSiteAs = SecondCleanserSite->GetAttributeSet();
	if (!SecondSiteAsc || !SecondSiteAs) return;

	// 약 참조 캡처로 안전한 람다 바인딩
	TWeakObjectPtr<UOverlayWidgetController> WeakThis(this);

	// 첫 번째 CleanserSite 체력 변경 바인딩
	FDelegateHandle FirstHealthHandle = FirstSiteAsc->GetGameplayAttributeValueChangeDelegate(FirstSiteAs->GetHealthAttribute()).AddLambda(
		[WeakThis](const FOnAttributeChangeData& Data)
		{
			if (UOverlayWidgetController* StrongThis = WeakThis.Get())
			{
				StrongThis->OnFirstCleanserHealthChanged.Broadcast(Data.NewValue);
			}
		}
	);
	CleanserSiteDelegateHandles.Add(TPair<TWeakObjectPtr<UAbilitySystemComponent>, FDelegateHandle>(FirstSiteAsc, FirstHealthHandle));

	FDelegateHandle FirstMaxHealthHandle = FirstSiteAsc->GetGameplayAttributeValueChangeDelegate(FirstSiteAs->GetMaxHealthAttribute()).AddLambda(
		[WeakThis](const FOnAttributeChangeData& Data)
		{
			if (UOverlayWidgetController* StrongThis = WeakThis.Get())
			{
				StrongThis->OnFirstCleanserMaxHealthChanged.Broadcast(Data.NewValue);
			}
		}
	);
	CleanserSiteDelegateHandles.Add(TPair<TWeakObjectPtr<UAbilitySystemComponent>, FDelegateHandle>(FirstSiteAsc, FirstMaxHealthHandle));

	// 두 번째 CleanserSite 체력 변경 바인딩
	FDelegateHandle SecondHealthHandle = SecondSiteAsc->GetGameplayAttributeValueChangeDelegate(SecondSiteAs->GetHealthAttribute()).AddLambda(
		[WeakThis](const FOnAttributeChangeData& Data)
		{
			if (UOverlayWidgetController* StrongThis = WeakThis.Get())
			{
				StrongThis->OnSecondCleanserHealthChanged.Broadcast(Data.NewValue);
			}
		}
	);
	CleanserSiteDelegateHandles.Add(TPair<TWeakObjectPtr<UAbilitySystemComponent>, FDelegateHandle>(SecondSiteAsc, SecondHealthHandle));

	FDelegateHandle SecondMaxHealthHandle = SecondSiteAsc->GetGameplayAttributeValueChangeDelegate(SecondSiteAs->GetMaxHealthAttribute()).AddLambda(
		[WeakThis](const FOnAttributeChangeData& Data)
		{
			if (UOverlayWidgetController* StrongThis = WeakThis.Get())
			{
				StrongThis->OnSecondCleanserMaxHealthChanged.Broadcast(Data.NewValue);
			}
		}
	);
	CleanserSiteDelegateHandles.Add(TPair<TWeakObjectPtr<UAbilitySystemComponent>, FDelegateHandle>(SecondSiteAsc, SecondMaxHealthHandle));

	// 초기값 UI 표시
	OnFirstCleanserHealthChanged.Broadcast(FirstSiteAs->GetHealth());
	OnFirstCleanserMaxHealthChanged.Broadcast(FirstSiteAs->GetMaxHealth());
	OnSecondCleanserHealthChanged.Broadcast(SecondSiteAs->GetHealth());
	OnSecondCleanserMaxHealthChanged.Broadcast(SecondSiteAs->GetMaxHealth());
}

