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
	ADRLobbyGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void TravelToStage(const FString& StageMapName, class ADRPlayerController* Requester);

protected:
	virtual void HandleWipeout() override;

	// 로비 재시작
	void RestartLobby();

private:
	void ExecuteTravel(const FString& StageMapName);
};
