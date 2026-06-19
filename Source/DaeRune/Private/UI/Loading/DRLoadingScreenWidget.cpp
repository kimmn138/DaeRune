// Copyright DaeRune


#include "UI/Loading/DRLoadingScreenWidget.h"

void UDRLoadingScreenWidget::NotifyIntroFinished()
{
	OnIntroFinished.Broadcast();
}

void UDRLoadingScreenWidget::NotifyOutroFinished()
{
	OnOutroFinished.Broadcast();
}
