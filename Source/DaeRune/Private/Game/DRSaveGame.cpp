// Copyright DaeRune


#include "Game/DRSaveGame.h"

const FString UDRSaveGame::SaveSlotName = TEXT("DaeRunePlayerProgress");
const int32 UDRSaveGame::UserIndex = 0;

UDRSaveGame::UDRSaveGame()
{
	bHasCompletedTutorial = false;
}
