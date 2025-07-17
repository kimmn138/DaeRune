// Copyright DaeRune


#include "UI/HUD/DRHUD.h"
#include "UI/Widget/DRUserWidget.h"
#include "UI/WidgetController/OverlayWidgetController.h"

UOverlayWidgetController* ADRHUD::GetOverlayWidgetController(const FWidgetControllerParams& WCParams) // 오버레이 컨트롤러 생성 및 반환 함수임
{
	if (OverlayWidgetController == nullptr)
	{
		// 컨트롤러 인스턴스 생성 및 파라미터 설정임
		OverlayWidgetController = NewObject<UOverlayWidgetController>(this, OverlayWidgetControllerClass);
		OverlayWidgetController->SetWidgetControllerParams(WCParams);
		OverlayWidgetController->BindCallbacksToDependencies();
	}
	return OverlayWidgetController; // 컨트롤러 반환임
}

void ADRHUD::InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS) // 오버레이 위젯 초기화 및 뷰포트 추가 함수임
{
	// 위젯 클래스 유효성 검사임
	checkf(OverlayWidgetClass, TEXT("Overlay Widget Class uninitialized, please fill out BP_DRHUD"));
	checkf(OverlayWidgetControllerClass, TEXT("Overlay Widget Controller Class uninitialized, please fill out BP_DRHUD"));

	// 위젯 생성 및 타입 캐스팅임
	UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), OverlayWidgetClass);
	OverlayWidget = Cast<UDRUserWidget>(Widget);

	// 위젯 컨트롤러 파라미터 설정용 구조체 생성임
	const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
	UOverlayWidgetController* WidgetController = GetOverlayWidgetController(WidgetControllerParams);

	// 위젯 컨트롤러 연결 및 초기값 브로드캐스트임
	OverlayWidget->SetWidgetController(WidgetController);
	WidgetController->BroadcastInitialValues();
	// 위젯 뷰포트에 추가임
	Widget->AddToViewport();
}
