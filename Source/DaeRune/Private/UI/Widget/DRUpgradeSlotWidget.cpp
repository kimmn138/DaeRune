// Copyright DaeRune


#include "UI/Widget/DRUpgradeSlotWidget.h"

void UDRUpgradeSlotWidget::SetSlotViewModel(const FDRSlotViewModel& InViewModel)
{
	ViewModel = InViewModel;
	OnViewModelUpdated();
}
