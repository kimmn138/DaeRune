// Copyright DaeRune


#include "UI/Loading/DRLoadingScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

const FName UDRLoadingScreenWidget::LoadingTextWidgetName(TEXT("Text_Loading"));

void UDRLoadingScreenWidget::SetLoadingMessage(const FText& Message)
{
	if (UTextBlock* TextBlock = WidgetTree ? WidgetTree->FindWidget<UTextBlock>(LoadingTextWidgetName) : nullptr)
	{
		TextBlock->SetText(Message);
	}
}

void UDRLoadingScreenWidget::NotifyIntroFinished()
{
	OnIntroFinished.Broadcast();
}

void UDRLoadingScreenWidget::NotifyOutroFinished()
{
	OnOutroFinished.Broadcast();
}
