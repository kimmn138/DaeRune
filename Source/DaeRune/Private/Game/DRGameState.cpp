// Copyright DaeRune


#include "Game/DRGameState.h"
#include "Player/DRPlayerState.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ADRGameState::ADRGameState()
{
	// 리플리케이션 활성화
	bReplicates = true;

	// 초기값 설정
	bIsGameInProgress = false;
	GameTime = 0.0f;
	AlivePlayersCount = 0;
	DeadPlayersCount = 0;
	TotalPlayersCount = 0;
	bIsGameOver = false;
	LastAlivePlayersCount = 0;
}

void ADRGameState::BeginPlay()
{
}

void ADRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
}

void ADRGameState::AddPlayerState(APlayerState* PlayerState)
{
}

void ADRGameState::RemovePlayerState(APlayerState* PlayerState)
{
}

void ADRGameState::SetRoomCode(const FString& NewRoomCode)
{
}

TArray<ADRPlayerState*> ADRGameState::GetDRPlayerArray() const
{
	return TArray<ADRPlayerState*>();
}

TArray<ADRPlayerState*> ADRGameState::GetAlivePlayerArray() const
{
	return TArray<ADRPlayerState*>();
}

void ADRGameState::StartGame()
{
}

void ADRGameState::EndGame(bool bVictory)
{
}

void ADRGameState::UpdateAlivePlayersCount()
{
}

bool ADRGameState::IsPlayerAlive(ADRPlayerState* PlayerState) const
{
	return false;
}

void ADRGameState::OnRep_RoomCode()
{
}

void ADRGameState::InitializeRoomCode()
{
}

void ADRGameState::UpdateGameTime()
{
}

void ADRGameState::CheckPlayersHealth()
{
}
