// Copyright DaeRune


#include "UI/Widget/DRUserWidget.h"

void UDRUserWidget::SetWidgetController(UObject* InWidgetController)
{
	WidgetController = InWidgetController;
	WidgetControllerSet();
}
