// Copyright DaeRune


#include "Game/DRStageGameMode.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"

ADRStageGameMode::ADRStageGameMode()
{
	// 기본 설정
	LobbyMapName = TEXT("StartupMap");
	WipeoutDelayTime = 3.0f;
}

void ADRStageGameMode::HandleWipeout()
{
	if (!HasAuthority()) return;

	// TODO: 패배 UI 표시, 패배 사운드 재생 등

	ReturnToLobby();
}

void ADRStageGameMode::ReturnToLobby()
{
	if (!HasAuthority()) return;

	UWorld* World = GetWorld();
	if (World)
	{
		bUseSeamlessTravel = true;
		World->ServerTravel(LobbyMapName + TEXT("?listen"));
	}

	// 플래그 리셋
	bIsWipeoutInProgress = false;
}

