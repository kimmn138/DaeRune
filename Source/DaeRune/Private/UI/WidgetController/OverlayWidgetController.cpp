// Copyright DaeRune


#include "UI/WidgetController/OverlayWidgetController.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"

void UOverlayWidgetController::BroadcastInitialValues()
{
	// 게임 시작 시 현재 어트리뷰트 값들을 UI에 전송
	OnHealthChanged.Broadcast(GetDRAS()->GetHealth());
	OnMaxHealthChanged.Broadcast(GetDRAS()->GetMaxHealth());
	OnWaterChanged.Broadcast(GetDRAS()->GetWater());
	OnMaxWaterChanged.Broadcast(GetDRAS()->GetMaxWater());
}

void UOverlayWidgetController::BindCallbacksToDependencies()
{
	// 콜백 바인딩
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

	// 어빌리티 정보 초기화 처리
	if (GetDRASC())
	{
		// 시작 어빌리티가 이미 부여되었다면 즉시 브로드캐스트
		if (GetDRASC()->bStartupAbilitiesGiven)
		{
			BroadcastAbilityInfo();
		}
		else
		{
			// 아직 부여되지 않았다면 부여 완료 시점에 브로드캐스트하도록 델리게이트 바인딩
			GetDRASC()->AbilitiesGivenDelegate.AddUObject(this, &UOverlayWidgetController::BroadcastAbilityInfo);
		}
	}
}

