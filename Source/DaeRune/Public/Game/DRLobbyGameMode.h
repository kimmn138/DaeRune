// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameModeBase.h"
#include "DRLobbyGameMode.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API ADRLobbyGameMode : public ADRGameModeBase
{
	GENERATED_BODY()
	
public:
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void TravelToStage(const FString& StageMapName, class ADRPlayerController* Requester);

private:
	void ExecuteTravel(const FString& StageMapName);
};
