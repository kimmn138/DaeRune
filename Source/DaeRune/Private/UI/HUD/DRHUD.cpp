// Copyright DaeRune


#include "UI/HUD/DRHUD.h"
#include "UI/Widget/DRUserWidget.h"
#include "UI/WidgetController/OverlayWidgetController.h"

UOverlayWidgetController* ADRHUD::GetOverlayWidgetController(const FWidgetControllerParams& WCParams)
{
	// 싱글톤 패턴: 한 번만 생성하고 재사용
	if (OverlayWidgetController == nullptr)
	{
		// 컨트롤러 생성 및 초기화
		OverlayWidgetController = NewObject<UOverlayWidgetController>(this, OverlayWidgetControllerClass);
		OverlayWidgetController->SetWidgetControllerParams(WCParams);
		OverlayWidgetController->BindCallbacksToDependencies();
	}
	return OverlayWidgetController;
}

void ADRHUD::InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
	// 블루프린트 클래스 설정 확인
	checkf(OverlayWidgetClass, TEXT("Overlay Widget Class uninitialized, please fill out BP_DRHUD"));
	checkf(OverlayWidgetControllerClass, TEXT("Overlay Widget Controller Class uninitialized, please fill out BP_DRHUD"));

	// 위젯 생성
	UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), OverlayWidgetClass);
	OverlayWidget = Cast<UDRUserWidget>(Widget);

	// WidgetController 파라미터 구성
	const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
	UOverlayWidgetController* WidgetController = GetOverlayWidgetController(WidgetControllerParams);

	// 위젯과 컨트롤러 연결
	OverlayWidget->SetWidgetController(WidgetController);
	// 초기 값들을 UI에 브로드캐스트
	WidgetController->BroadcastInitialValues();
	// 화면에 위젯 추가
	Widget->AddToViewport();
}

void ADRHUD::UpdateOverlayForSpectating(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
	// 기존 WidgetController 파괴
	if (OverlayWidgetController)
	{
		OverlayWidgetController->UnbindAllDelegates();

		OverlayWidgetController->ConditionalBeginDestroy();
		OverlayWidgetController = nullptr;
	}

	// 새로운 파라미터로 WidgetController 생성
	const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
	UOverlayWidgetController* WidgetController = GetOverlayWidgetController(WidgetControllerParams);

	// 기존 위젯에 새 컨트롤러 연결
	if (OverlayWidget)
	{
		OverlayWidget->SetWidgetController(WidgetController);
		WidgetController->BroadcastInitialValues();
		WidgetController->BroadcastAbilityInfo();
	}
}
