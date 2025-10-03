// Copyright DaeRune


#include "Game/DRGameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework//PlayerState.h"
#include "Engine/World.h"

ADRGameStateBase::ADRGameStateBase()
{
	// 리플리케이션 활성화
	bReplicates = true;

	// 초기값 설정
	CurrentPlayerCount = 0;
	MaxPlayerCount = 4;  // 4인 협동 게임
}

void ADRGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRGameStateBase, CurrentPlayerCount);
	DOREPLIFETIME(ADRGameStateBase, MaxPlayerCount);
}

void ADRGameStateBase::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);

	if (HasAuthority())
	{
		UpdatePlayerCount();
	}
}

void ADRGameStateBase::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);

	if (HasAuthority())
	{
		UpdatePlayerCount();
	}
}

APlayerState* ADRGameStateBase::GetHostPlayer() const
{
	// 호스트는 보통 PlayerID가 0
	for (APlayerState* PS : PlayerArray)
	{
		if (PS->GetPlayerId() == 0)
		{
			return PS;
		}
	}

	// 못 찾으면 첫 번째 플레이어를 호스트로
	if (PlayerArray.Num() > 0)
	{
		return PlayerArray[0];
	}

	return nullptr;
}

bool ADRGameStateBase::IsPlayerHost(APlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return false;
	}

	return PlayerState == GetHostPlayer();
}

void ADRGameStateBase::UpdatePlayerCount()
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentPlayerCount = 0;

	for (APlayerState* PS : PlayerArray)
	{
		if (PS && !PS->IsSpectator())
		{
			CurrentPlayerCount++;
		}
	}
}
