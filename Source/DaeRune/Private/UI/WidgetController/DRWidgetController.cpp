// Copyright DaeRune


#include "UI/WidgetController/DRWidgetController.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"

void UDRWidgetController::SetWidgetControllerParams(const FWidgetControllerParams& WCParams)
{
	// �Ķ���� ����ü���� �� ������Ʈ �Ҵ�
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
	// ���� �����Ƽ�� ���� �ο����� �ʾ����� ���
	if (!GetDRASC()->bStartupAbilitiesGiven) return;

	// �� �����Ƽ ������ ��ȸ�ϸ� UI�� ����
	FForEachAbility BroadcastDelegate;
	BroadcastDelegate.BindLambda([this](const FGameplayAbilitySpec& AbilitySpec)
	{
		// AbilitySpec���� �±� ���� ����
		FDRAbilityInfo Info = AbilityInfo->FindAbilityInfoForTag(DRAbilitySystemComponent->GetAbilityTagFromSpec(AbilitySpec));
		Info.InputTag = DRAbilitySystemComponent->GetInputTagFromSpec(AbilitySpec);
		// UI�� �����Ƽ ���� ��ε�ĳ��Ʈ
		AbilityInfoDelegate.Broadcast(Info);
	});
	// ASC�� ��� �����Ƽ�� ���� ���� �Լ� ����
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
