// Copyright DaeRune


#include "Game/DRGameInstance.h"
#include "Game/DRSaveGame.h"
#include "Kismet/GameplayStatics.h"

void UDRGameInstance::Init()
{
	Super::Init();
	LoadProgress();
}

bool UDRGameInstance::HasCompletedTutorial() const
{
	if (!CurrentSaveGame) return false;
	return CurrentSaveGame->bHasCompletedTutorial;
}

void UDRGameInstance::SetTutorialCompleted()
{
	if (!CurrentSaveGame)
	{
		LoadProgress();
	}
	CurrentSaveGame->bHasCompletedTutorial = true;
	SaveProgress();
}

void UDRGameInstance::LoadProgress()
{
	if (UGameplayStatics::DoesSaveGameExist(UDRSaveGame::SaveSlotName, UDRSaveGame::UserIndex))
	{
		CurrentSaveGame = Cast<UDRSaveGame>(
			UGameplayStatics::LoadGameFromSlot(UDRSaveGame::SaveSlotName, UDRSaveGame::UserIndex));
	}

	if (!CurrentSaveGame)
	{
		CurrentSaveGame = Cast<UDRSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UDRSaveGame::StaticClass()));
	}
}

void UDRGameInstance::SaveProgress()
{
	if (CurrentSaveGame)
	{
		UGameplayStatics::SaveGameToSlot(CurrentSaveGame, UDRSaveGame::SaveSlotName, UDRSaveGame::UserIndex);
	}
}
