// Copyright DaeRune


#include "UI/Widget/DRUpgradeChipWidget.h"

void UDRUpgradeChipWidget::SetChipViewModel(const FDRChipViewModel& InViewModel)
{
	ViewModel = InViewModel;
	OnViewModelUpdated();
}
