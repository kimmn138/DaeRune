// Copyright DaeRune


#include "UI/Widget/DRUserWidget.h"

void UDRUserWidget::SetWidgetController(UObject* InWidgetController)
{
	// WidgetController 참조 저장
	WidgetController = InWidgetController;
	// 블루프린트 구현 이벤트 호출
	WidgetControllerSet();
}
