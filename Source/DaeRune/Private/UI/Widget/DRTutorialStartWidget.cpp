// Copyright DaeRune


#include "UI/Widget/DRTutorialStartWidget.h"
#include "Game/DRMainMenuGameMode.h"
#include "Player/DRPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UDRTutorialStartWidget::OnStartGameClicked()
{
	UWorld* World = GetWorld();
	if (!World) return;

	ADRMainMenuGameMode* GM = Cast<ADRMainMenuGameMode>(World->GetAuthGameMode());
	if (GM)
	{
		GM->StartTutorial();
	}
}

void UDRTutorialStartWidget::OnSettingsClicked()
{
	APlayerController* PC = GetOwningPlayer();
	if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(PC))
	{
		DRPC->OpenSettingsMenu();
	}
}

void UDRTutorialStartWidget::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayer(), EQuitPreference::Quit, false);
}
