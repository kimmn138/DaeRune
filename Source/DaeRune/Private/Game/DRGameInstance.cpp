// Copyright DaeRune


#include "Game/DRGameInstance.h"
#include "Game/DRSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DRPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

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

void UDRGameInstance::SavePlayerClassSelection(const FString& PlayerName, EPlayerCharacterClass SelectedClass)
{
	PlayerClassSelections.Add(PlayerName, SelectedClass);
}

EPlayerCharacterClass UDRGameInstance::LoadPlayerClassSelection(const FString& PlayerName) const
{
	const EPlayerCharacterClass* Found = PlayerClassSelections.Find(PlayerName);
	return Found ? *Found : EPlayerCharacterClass::GardenRobot;
}

void UDRGameInstance::SaveAllPlayerSelections(UWorld* World)
{
	if (!World) return;

	AGameStateBase* GS = World->GetGameState<AGameStateBase>();
	if (!GS) return;

	PlayerClassSelections.Empty();
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (ADRPlayerState* DRPS = Cast<ADRPlayerState>(PS))
		{
			PlayerClassSelections.Add(DRPS->GetPlayerName(), DRPS->GetSelectedPlayerClass());
		}
	}
}

void UDRGameInstance::ClearPlayerClassSelections()
{
	PlayerClassSelections.Empty();
}

void UDRGameInstance::SaveProgress()
{
	if (CurrentSaveGame)
	{
		UGameplayStatics::SaveGameToSlot(CurrentSaveGame, UDRSaveGame::SaveSlotName, UDRSaveGame::UserIndex);
	}
}

