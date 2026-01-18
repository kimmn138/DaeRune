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

	// 맵 전환 전 오디오 정리
	void CleanupAllAudioBeforeTravel();

protected:
	virtual void BeginPlay() override;
	virtual void HandleWipeout() override;

	// 세션 도중 참가 허용
	void AllowJoinInProgress();

	// 로비 재시작
	void RestartLobby();

private:
	void ExecuteTravel(const FString& StageMapName);
};
