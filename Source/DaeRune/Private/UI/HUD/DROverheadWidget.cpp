// Copyright DaeRune


#include "UI/HUD/DROverheadWidget.h"
#include "Components/TextBlock.h"

void UDROverheadWidget::SetDisplayText(FString TextToDisplay)
{
	if (DisplayText)
	{
		DisplayText->SetText(FText::FromString(TextToDisplay));
	}
}

void UDROverheadWidget::NativeDestruct()
{
	RemoveFromParent();

	Super::NativeDestruct();
}
