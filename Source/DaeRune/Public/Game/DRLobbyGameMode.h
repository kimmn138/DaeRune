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

	// SeamlessTravel로 돌아온 플레이어 처리
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void TravelToStage(const FString& StageMapName, class ADRPlayerController* Requester);

protected:
	virtual void BeginPlay() override;
	virtual void HandleWipeout() override;

	// ���� ���� ���� ���
	void AllowJoinInProgress();

	// �κ� �����
	void RestartLobby();

private:
	void ExecuteTravel(const FString& StageMapName);
};
