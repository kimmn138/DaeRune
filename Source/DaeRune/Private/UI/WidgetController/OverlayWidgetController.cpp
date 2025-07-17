// Copyright DaeRune


#include "UI/WidgetController/OverlayWidgetController.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"

void UOverlayWidgetController::BroadcastInitialValues() // 초기 UI 속성 값 브로드캐스트 구현부임
{
	OnHealthChanged.Broadcast(GetDRAS()->GetHealth()); // 초기 체력 값 방송임
	OnMaxHealthChanged.Broadcast(GetDRAS()->GetMaxHealth()); // 초기 최대 체력 값 방송임
}

void UOverlayWidgetController::BindCallbacksToDependencies() // 콜백 바인딩 구현부임
{
	// 체력 속성 변경 시 콜백 등록임
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GetDRAS()->GetHealthAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnHealthChanged.Broadcast(Data.NewValue); // 변경된 체력 값 방송임
		}
	);

	// 최대 체력 속성 변경 시 콜백 등록임
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GetDRAS()->GetMaxHealthAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnMaxHealthChanged.Broadcast(Data.NewValue); // 변경된 최대 체력 방송임
		}
	);

	if (GetDRASC()) // DR ASC 유효성 검사임
	{
		if (GetDRASC()->bStartupAbilitiesGiven) // 시작 능력 부여 여부 검사임
		{
			BroadcastAbilityInfo(); // 즉시 능력 정보 방송임
		}
		else // 시작 전능력 대기 상태임
		{
			GetDRASC()->AbilitiesGivenDelegate.AddUObject(this, &UOverlayWidgetController::BroadcastAbilityInfo); // 부여 후 방송 등록임
		}

		// 효과 태그 수신 시 메시지 위젯 처리 임
		GetDRASC()->EffectAssetTags.AddLambda(
			[this](const FGameplayTagContainer& AssetTags)
			{
				for (const FGameplayTag& Tag : AssetTags) // 각 태그 순회임
				{
					FGameplayTag MessageTag = FGameplayTag::RequestGameplayTag(FName("Message")); // 메시지 카테고리 태그임
					if (Tag.MatchesTag(MessageTag)) // 메시지 태그 매칭 검사임
					{
						const FUIWidgetRow* Row = GetDataTableRowByTag<FUIWidgetRow>(MessageWidgetDataTable, Tag); // 행 조회 임
						MessageWidgetRowDelegate.Broadcast(*Row); // 메시지 위젯 행 방송임
					}
				}
			}
		);
	}
}

