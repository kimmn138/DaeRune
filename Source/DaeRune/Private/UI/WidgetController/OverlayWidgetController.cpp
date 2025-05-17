// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WidgetController/OverlayWidgetController.h"
#include "AbilitySystem/DRAttributeSet.h"

void UOverlayWidgetController::BroadcastInitialValues()
{
	const UDRAttributeSet* DRAttributeSet = CastChecked<UDRAttributeSet>(AttributeSet);

	OnHealthChanged.Broadcast(DRAttributeSet->GetHealth());
	OnMaxHealthChanged.Broadcast(DRAttributeSet->GetMaxHealth());
}
