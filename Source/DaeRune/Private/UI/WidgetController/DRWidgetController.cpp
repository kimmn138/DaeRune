// Copyright DaeRune


#include "UI/WidgetController/DRWidgetController.h"

void UDRWidgetController::SetWidgetControllerParams(const FWidgetControllerParams& WCParams)
{
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
