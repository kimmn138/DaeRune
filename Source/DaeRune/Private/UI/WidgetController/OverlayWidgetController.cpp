// Copyright DaeRune


#include "UI/WidgetController/OverlayWidgetController.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/Data/StatusEffectInfo.h"
#include "Game/DRStageGameState.h"
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
}

void UOverlayWidgetController::HandlePhaseObjectiveChanged()
{
	ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
    if (!DRGameState) return;
    
    FPhaseObjectiveData ObjectiveData = DRGameState->GetCurrentPhaseObjective();
    int32 CurrentProgress = DRGameState->GetCurrentObjectiveProgress();
	
	OnObjectiveTextChanged.Broadcast(ObjectiveData.ObjectiveTitle,ObjectiveData.ProgressFormat);
	OnObjectiveProgressChanged.Broadcast(CurrentProgress,ObjectiveData.RequiredCount);
}

void UOverlayWidgetController::BindPhaseObjectiveDelegate()
{
	ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
    
	if (DRGameState)
	{
		// 델리게이트 바인딩
		DRGameState->OnPhaseObjectiveChangedDelegate.AddLambda(
			[this]()
			{
				HandlePhaseObjectiveChanged();
			}
		);
        
		// 현재 값 즉시 받아오기
		HandlePhaseObjectiveChanged();
	}
}

