// Copyright DaeRune


#include "UI/WidgetController/OverlayWidgetController.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Game/DRStageGameState.h"
#include "Phase/DRPhaseBase.h"

void UOverlayWidgetController::BroadcastInitialValues()
{
	// ���� ���� �� ���� ��Ʈ����Ʈ ������ UI�� ����
	OnHealthChanged.Broadcast(GetDRAS()->GetHealth());
	OnMaxHealthChanged.Broadcast(GetDRAS()->GetMaxHealth());
	OnWaterChanged.Broadcast(GetDRAS()->GetWater());
	OnMaxWaterChanged.Broadcast(GetDRAS()->GetMaxWater());

	// 디버프 초기 상태
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
    
	OnBleedDebuffChanged.Broadcast(AbilitySystemComponent->HasMatchingGameplayTag(GameplayTags.Debuff_Bleed));

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

	// 디버프 태그 바인딩
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();

	AbilitySystemComponent->RegisterGameplayTagEvent(GameplayTags.Debuff_Bleed, EGameplayTagEventType::NewOrRemoved).AddLambda(
		[this](const FGameplayTag Tag, int32 NewCount)
		{
			OnBleedDebuffChanged.Broadcast(NewCount > 0);
		}
	);

	// �����Ƽ ���� �ʱ�ȭ ó��
	if (GetDRASC())
	{
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
	if (ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>())
	{
		UE_LOG(LogTemp, Log, TEXT("Hello Bind"));
		DRGameState->OnPhaseObjectiveChangedDelegate.AddLambda(
			[this]()
			{
				HandlePhaseObjectiveChanged();
			}
		);
	}
}

void UOverlayWidgetController::HandlePhaseObjectiveChanged()
{
	ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
    if (!DRGameState) return;
    
    FPhaseObjectiveData ObjectiveData = DRGameState->GetCurrentPhaseObjective();
    int32 CurrentProgress = DRGameState->GetCurrentObjectiveProgress();

	UE_LOG(LogTemp, Log, TEXT("Bye Bind"));
	
	OnObjectiveTextChanged.Broadcast(ObjectiveData.ObjectiveTitle,ObjectiveData.ProgressFormat);
	OnObjectiveProgressChanged.Broadcast(CurrentProgress,ObjectiveData.RequiredCount);
}

