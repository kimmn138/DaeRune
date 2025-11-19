// Copyright DaeRune


#include "UI/WidgetController/OverlayWidgetController.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/DRCleanserSiteAttributeSet.h"
#include "AbilitySystem/Data/StatusEffectInfo.h"
#include "Actor/DRCleanserSite.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "Phase/DRPhase3.h"
#include "Phase/DRPhaseBase.h"

void UOverlayWidgetController::BroadcastInitialValues()
{
	// ���� ���� �� ���� ��Ʈ����Ʈ ������ UI�� ����
	OnHealthChanged.Broadcast(GetDRAS()->GetHealth());
	OnMaxHealthChanged.Broadcast(GetDRAS()->GetMaxHealth());
	OnWaterChanged.Broadcast(GetDRAS()->GetWater());
	OnMaxWaterChanged.Broadcast(GetDRAS()->GetMaxWater());

	// 페이즈 목표 초기값 추가
	HandlePhaseObjectiveChanged();
}

void UOverlayWidgetController::BindCallbacksToDependencies()
{
	// �ݹ� ���ε�
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GetDRAS()->GetHealthAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnHealthChanged.Broadcast(Data.NewValue);
		}
	);

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GetDRAS()->GetMaxHealthAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnMaxHealthChanged.Broadcast(Data.NewValue);
		}
	);

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GetDRAS()->GetWaterAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnWaterChanged.Broadcast(Data.NewValue);
		}
	);

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GetDRAS()->GetMaxWaterAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnMaxWaterChanged.Broadcast(Data.NewValue);
		}
	);
	

	// �����Ƽ ���� �ʱ�ȭ ó��
	if (GetDRASC())
	{
		GetDRASC()->EffectAssetTags.AddLambda(
			[this](const FGameplayTagContainer& AssetTags, bool HasDuration, const float Duration, bool DisplayStackCount, const int32 StackCount)
			{
				FGameplayTag BuffTag = FGameplayTag::RequestGameplayTag(FName("Buff"));
				FGameplayTag DebuffTag = FGameplayTag::RequestGameplayTag(FName("Debuff"));
				for (const FGameplayTag& Tag : AssetTags)
				{
					if (Tag.MatchesTag(BuffTag))
					{
						FEffectInfo EffectInfo = StatusEffectData->FindEffectInfoForTag(Tag);
						if (EffectInfo.EffectTag.IsValid())
						{
							EffectInfo.bHasDuration = HasDuration;
							EffectInfo.Duration = Duration;
							EffectInfo.bDisplayStack = DisplayStackCount;
							EffectInfo.StackCount = StackCount;
							StatusEffectWidgetDelegate.Broadcast(EffectInfo);
						}
					}
					if (Tag.MatchesTag(DebuffTag))
					{
						FEffectInfo EffectInfo = StatusEffectData->FindEffectInfoForTag(Tag);
						if (EffectInfo.EffectTag.IsValid())
						{
							EffectInfo.bHasDuration = HasDuration;
							EffectInfo.Duration = Duration;
							EffectInfo.bDisplayStack = DisplayStackCount;
							EffectInfo.StackCount = StackCount;
							EffectInfo.bIsDebuff = true;
							StatusEffectWidgetDelegate.Broadcast(EffectInfo);
						}
					}
				}
			}
		);

		GetDRASC()->EffectRemovedDelegate.AddLambda(
			[this](const FGameplayTagContainer& AssetTags)
			{
				FGameplayTag BuffTag = FGameplayTag::RequestGameplayTag(FName("Buff"));
				FGameplayTag DebuffTag = FGameplayTag::RequestGameplayTag(FName("Debuff"));
				for (const FGameplayTag& Tag : AssetTags)
				{
					if (Tag.MatchesTag(BuffTag))
					{
						EffectTagRemovedDelegate.Broadcast(Tag, false);
					}
					if (Tag.MatchesTag(DebuffTag))
					{
						EffectTagRemovedDelegate.Broadcast(Tag, true);
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
	}

	// GameState 페이즈 목표 델리게이트 바인딩
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PhaseBindingDelayTimer,
			this,
			&UOverlayWidgetController::BindPhaseObjectiveDelegate,
			1.0f,  // 1.0초 대기
			false  // 한 번만 실행
		);
	}

	// Phase 변경 델리게이트 바인딩
	if (ADRStageGameState* DRStageGameState = GetWorld()->GetGameState<ADRStageGameState>())
	{
		DRStageGameState->OnPhaseChangedDelegate.AddDynamic(this, &UOverlayWidgetController::OnPhaseChanged);
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

void UOverlayWidgetController::HandlePhaseObjectiveChanged()
{
	const ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
    if (!DRGameState) return;

    const FPhaseObjectiveData ObjectiveData = DRGameState->GetCurrentPhaseObjective();
    int32 CurrentProgress = DRGameState->GetCurrentObjectiveProgress();
	
	OnObjectiveTextChanged.Broadcast(ObjectiveData.ObjectiveTitle,ObjectiveData.ProgressFormat);
	OnObjectiveProgressChanged.Broadcast(CurrentProgress,ObjectiveData.RequiredCount);
}

void UOverlayWidgetController::BindPhaseObjectiveDelegate()
{
	if (ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>())
	{
		// 델리게이트 바인딩
		DRGameState->OnPhaseObjectiveChangedDelegate.AddLambda(
			[this]()
			{
				HandlePhaseObjectiveChanged();

				// Phase가 변경될 때마다 웨이브 타이머 바인딩 체크
				CheckAndBindWaveTimer();
			}
		);
        
		// 현재 값 즉시 받아오기
		HandlePhaseObjectiveChanged();
	}
}

void UOverlayWidgetController::BindWaveTimerDelegate()
{
	ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
	if (!DRGameState) return;
	
	// 웨이브 타이머 델리게이트 바인딩
	DRGameState->OnWaveTimerChangedDelegate.AddLambda(
		[this](int32 WaveNumber, float RemainingTime, bool bIsRestTime)
		{
			OnWaveTimerChanged.Broadcast(WaveNumber, RemainingTime, bIsRestTime);
		}
	);
	
	// 초기값 즉시 브로드캐스트
	OnWaveTimerChanged.Broadcast(
		DRGameState->GetCurrentWaveNumber(),
		DRGameState->GetWaveRemainingTime(),
		DRGameState->IsWaveRestTime()
	);
}

void UOverlayWidgetController::CheckAndBindWaveTimer()
{
	ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
	if (!DRGameState) return;
	
	const int32 CurrentPhase = DRGameState->GetCurrentPhaseIndex();
	
	// Phase 3일 때만 바인딩 (인덱스 2)
	if (CurrentPhase == 2)
	{
		// 이미 바인딩되었는지 체크
		if (CachedPhaseNumber != 2)
		{
			CachedPhaseNumber = 2;
			BindWaveTimerDelegate();
		}
	}
	else
	{
		// Phase 3가 아니면 캐시 초기화
		if (CachedPhaseNumber == 2)
		{
			CachedPhaseNumber = -1;
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

	// 체력 변경 바인딩
	FirstSiteAsc->GetGameplayAttributeValueChangeDelegate(FirstSiteAs->GetHealthAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnFirstCleanserHealthChanged.Broadcast(Data.NewValue);
		}
	);

	FirstSiteAsc->GetGameplayAttributeValueChangeDelegate(FirstSiteAs->GetMaxHealthAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnFirstCleanserMaxHealthChanged.Broadcast(Data.NewValue);
		}
	);

	// 체력 변경 바인딩
	SecondSiteAsc->GetGameplayAttributeValueChangeDelegate(SecondSiteAs->GetHealthAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnSecondCleanserHealthChanged.Broadcast(Data.NewValue);
		}
	);		

	SecondSiteAsc->GetGameplayAttributeValueChangeDelegate(SecondSiteAs->GetMaxHealthAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnSecondCleanserMaxHealthChanged.Broadcast(Data.NewValue);
		}
	);

	// 초기값 UI 표시
	OnFirstCleanserHealthChanged.Broadcast(FirstSiteAs->GetHealth());
	OnFirstCleanserMaxHealthChanged.Broadcast(FirstSiteAs->GetMaxHealth());
	OnSecondCleanserHealthChanged.Broadcast(SecondSiteAs->GetHealth());
	OnSecondCleanserMaxHealthChanged.Broadcast(SecondSiteAs->GetMaxHealth());
}

