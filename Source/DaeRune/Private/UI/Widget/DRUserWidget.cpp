// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Widget/DRUserWidget.h"

void UDRUserWidget::SetWidgetController(UObject* InWidgetController)
{
	WidgetController = InWidgetController;
	WidgetControllerSet();
}
