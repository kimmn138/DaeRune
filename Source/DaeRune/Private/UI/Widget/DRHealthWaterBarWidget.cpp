// Copyright DaeRune


#include "UI/Widget/DRHealthWaterBarWidget.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UI/WidgetController/OverlayWidgetController.h"

void UDRHealthWaterBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Image_Water)
	{
		DefaultWaterFrame = Image_Water->GetBrush().GetResourceObject();
	}
}

void UDRHealthWaterBarWidget::NativeDestruct()
{
	UnbindController();

	Super::NativeDestruct();
}

void UDRHealthWaterBarWidget::NativeWidgetControllerSet()
{
	UnbindController();

	UOverlayWidgetController* Overlay = Cast<UOverlayWidgetController>(WidgetController);
	if (!Overlay) return;

	Overlay->OnWaterTankExpandedChanged.AddUniqueDynamic(this, &UDRHealthWaterBarWidget::HandleWaterTankExpandedChanged);
	BoundController = Overlay;

	// 컨트롤러가 초기값을 이미 방송했을 수도 있으니 지금 상태로 한 번 칠한다
	HandleWaterTankExpandedChanged(Overlay->IsWaterTankExpanded());
}

void UDRHealthWaterBarWidget::UnbindController()
{
	if (UOverlayWidgetController* Old = BoundController.Get())
	{
		Old->OnWaterTankExpandedChanged.RemoveDynamic(this, &UDRHealthWaterBarWidget::HandleWaterTankExpandedChanged);
	}
	BoundController.Reset();
}

void UDRHealthWaterBarWidget::HandleWaterTankExpandedChanged(bool bExpanded)
{
	if (!Image_Water || !ExpandedWaterFrame) return;

	UObject* Frame = bExpanded ? ExpandedWaterFrame.Get() : DefaultWaterFrame.Get();
	if (Frame && Image_Water->GetBrush().GetResourceObject() != Frame)
	{
		// 브러시 크기(ImageSize)는 그대로 두고 그림만 바꾼다 — Water_150 은 기존 테두리와 같은 529px
		Image_Water->SetBrushResourceObject(Frame);
	}
}
