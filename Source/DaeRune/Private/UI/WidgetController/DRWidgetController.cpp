// Copyright DaeRune


#include "UI/WidgetController/DRWidgetController.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"

void UDRWidgetController::SetWidgetControllerParams(const FWidgetControllerParams& WCParams)
{
	PlayerController = WCParams.PlayerController; // 플레이어 컨트롤러 저장임
	PlayerState = WCParams.PlayerState; // 플레이어 스테이트 저장임
	AbilitySystemComponent = WCParams.AbilitySystemComponent; // ASC 저장임
	AttributeSet = WCParams.AttributeSet; // 어트리뷰트 세트 저장임
}

void UDRWidgetController::BroadcastInitialValues()
{
}

void UDRWidgetController::BindCallbacksToDependencies()
{
}

void UDRWidgetController::BroadcastAbilityInfo()
{
	if (!GetDRASC()->bStartupAbilitiesGiven) return; // 시작 능력 부여 여부 체크임

	FForEachAbility BroadcastDelegate; // 각 능력 순회 델리게이트임
	BroadcastDelegate.BindLambda([this](const FGameplayAbilitySpec& AbilitySpec)
	{
		FDRAbilityInfo Info = AbilityInfo->FindAbilityInfoForTag(DRAbilitySystemComponent->GetAbilityTagFromSpec(AbilitySpec)); // 능력 정보 조회임
		Info.InputTag = DRAbilitySystemComponent->GetInputTagFromSpec(AbilitySpec); // 입력 태그 설정임
		AbilityInfoDelegate.Broadcast(Info); // 델리게이트 호출임
	});
	GetDRASC()->ForEachAbility(BroadcastDelegate); // 모든 능력에 대해 브로드캐스트임
}

ADRPlayerController* UDRWidgetController::GetDRPC()
{
	if (DRPlayerController == nullptr)
	{
		DRPlayerController = Cast<ADRPlayerController>(PlayerController); // 캐스팅 후 캐시임
	}
	return DRPlayerController; // DR 플레이어 컨트롤러 반환임
}

ADRPlayerState* UDRWidgetController::GetDRPS()
{
	if (DRPlayerState == nullptr)
	{
		DRPlayerState = Cast<ADRPlayerState>(PlayerState); // 캐스팅 후 캐시임
	}
	return DRPlayerState; // DR 플레이어 스테이트 반환임
}

UDRAbilitySystemComponent* UDRWidgetController::GetDRASC()
{
	if (DRAbilitySystemComponent == nullptr)
	{
		DRAbilitySystemComponent = Cast<UDRAbilitySystemComponent>(AbilitySystemComponent); // 캐스팅 후 캐시임
	}
	return DRAbilitySystemComponent; // DR ASC 반환임
}

UDRAttributeSet* UDRWidgetController::GetDRAS()
{
	if (DRAttributeSet == nullptr)
	{
		DRAttributeSet = Cast<UDRAttributeSet>(AttributeSet); // 캐스팅 후 캐시임
	}
	return DRAttributeSet; // DR 어트리뷰트 세트 반환임
}
