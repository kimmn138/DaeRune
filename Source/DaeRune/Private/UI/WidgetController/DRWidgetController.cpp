// Copyright DaeRune


#include "UI/WidgetController/DRWidgetController.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"

void UDRWidgetController::SetWidgetControllerParams(const FWidgetControllerParams& WCParams)
{
	// 파라미터 구조체에서 각 컴포넌트 할당
	PlayerController = WCParams.PlayerController;
	PlayerState = WCParams.PlayerState;
	AbilitySystemComponent = WCParams.AbilitySystemComponent;
	AttributeSet = WCParams.AttributeSet;
}

void UDRWidgetController::BroadcastInitialValues()
{
}

void UDRWidgetController::BindCallbacksToDependencies()
{
}

void UDRWidgetController::BroadcastAbilityInfo()
{
	// 시작 어빌리티가 아직 부여되지 않았으면 대기
	if (!GetDRASC()->bStartupAbilitiesGiven) return;

	// 각 어빌리티 정보를 순회하며 UI에 전달
	FForEachAbility BroadcastDelegate;
	BroadcastDelegate.BindLambda([this](const FGameplayAbilitySpec& AbilitySpec)
	{
		// AbilitySpec에서 태그 정보 추출
		FDRAbilityInfo Info = AbilityInfo->FindAbilityInfoForTag(DRAbilitySystemComponent->GetAbilityTagFromSpec(AbilitySpec));
		Info.InputTag = DRAbilitySystemComponent->GetInputTagFromSpec(AbilitySpec);
		// UI에 어빌리티 정보 브로드캐스트
		AbilityInfoDelegate.Broadcast(Info);
	});
	// ASC의 모든 어빌리티에 대해 람다 함수 실행
	GetDRASC()->ForEachAbility(BroadcastDelegate);
}

ADRPlayerController* UDRWidgetController::GetDRPC()
{
	if (DRPlayerController == nullptr)
	{
		DRPlayerController = Cast<ADRPlayerController>(PlayerController);
	}
	return DRPlayerController;
}

ADRPlayerState* UDRWidgetController::GetDRPS()
{
	if (DRPlayerState == nullptr)
	{
		DRPlayerState = Cast<ADRPlayerState>(PlayerState);
	}
	return DRPlayerState;
}

UDRAbilitySystemComponent* UDRWidgetController::GetDRASC()
{
	if (DRAbilitySystemComponent == nullptr)
	{
		DRAbilitySystemComponent = Cast<UDRAbilitySystemComponent>(AbilitySystemComponent);
	}
	return DRAbilitySystemComponent;
}

UDRAttributeSet* UDRWidgetController::GetDRAS()
{
	if (DRAttributeSet == nullptr)
	{
		DRAttributeSet = Cast<UDRAttributeSet>(AttributeSet);
	}
	return DRAttributeSet;
}
