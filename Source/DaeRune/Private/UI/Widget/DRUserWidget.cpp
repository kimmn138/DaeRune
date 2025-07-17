// Copyright DaeRune


#include "UI/Widget/DRUserWidget.h"

void UDRUserWidget::SetWidgetController(UObject* InWidgetController) // 위젯 컨트롤러 저장 및 이벤트 호출 함수임
{
	WidgetController = InWidgetController; // 위젯 컨트롤러 저장임
	WidgetControllerSet(); // 설정 완료 이벤트 트리거임
}
