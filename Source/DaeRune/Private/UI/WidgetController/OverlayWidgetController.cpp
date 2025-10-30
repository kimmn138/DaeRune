// Copyright DaeRune


#include "UI/WidgetController/OverlayWidgetController.h"
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
    if (!DRGameState) 
    {
        UE_LOG(LogTemp, Warning, TEXT("HandlePhaseObjectiveChanged: GameState is null"));
        return;
    }
    
    FPhaseObjectiveData ObjectiveData = DRGameState->GetCurrentPhaseObjective();
    int32 CurrentProgress = DRGameState->GetCurrentObjectiveProgress();
    
    // 디버깅 로그
    UE_LOG(LogTemp, Warning, TEXT("PhaseNumber: %d"), ObjectiveData.PhaseNumber);
    UE_LOG(LogTemp, Warning, TEXT("ObjectiveTitle: %s"), *ObjectiveData.ObjectiveTitle.ToString());
    UE_LOG(LogTemp, Warning, TEXT("ProgressFormat: %s"), *ObjectiveData.ProgressFormat.ToString());
    
    // ⚡ 중요한 수정: 텍스트가 비어있으면 캐싱하지 않고 리턴
    if (ObjectiveData.ObjectiveTitle.IsEmpty() || ObjectiveData.ProgressFormat.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Objective texts are empty! Skipping update."));
        return;  // 비어있으면 캐싱도 하지 않고 그냥 리턴!
    }
    
    // 텍스트 업데이트 (PhaseNumber 체크 + 강제 업데이트 조건 추가)
    if (ObjectiveData.PhaseNumber != CachedPhaseNumber || 
        (CachedPhaseNumber == ObjectiveData.PhaseNumber && !CachedObjectiveTitle.EqualTo(ObjectiveData.ObjectiveTitle)))
    {
        CachedPhaseNumber = ObjectiveData.PhaseNumber;
        CachedObjectiveTitle = ObjectiveData.ObjectiveTitle;  // 텍스트도 캐싱
        
        UE_LOG(LogTemp, Warning, TEXT("Broadcasting texts: Title=%s, Format=%s"), 
            *ObjectiveData.ObjectiveTitle.ToString(), 
            *ObjectiveData.ProgressFormat.ToString());
            
        OnObjectiveTextChanged.Broadcast(
            ObjectiveData.ObjectiveTitle,
            ObjectiveData.ProgressFormat
        );
    }
    
    // 진행도 업데이트
    if (CurrentProgress != CachedProgress || ObjectiveData.RequiredCount != CachedRequiredCount)
    {
        CachedProgress = CurrentProgress;
        CachedRequiredCount = ObjectiveData.RequiredCount;
        
        OnObjectiveProgressChanged.Broadcast(
            CurrentProgress,
            ObjectiveData.RequiredCount
        );
    }
}

