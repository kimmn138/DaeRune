// Copyright DaeRune


#include "Game/DRGameModeBase.h"
#include "Character/DRCharacterBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

ADRGameModeBase::ADRGameModeBase()
{
	WipeoutDelayTime = 3.0f;
	bIsWipeoutInProgress = false;
}

void ADRGameModeBase::OnPlayerDied(APlayerState* DeadPlayer)
{
	if (!HasAuthority()) return;

	if (!DeadPlayer) return;

	// 이미 전멸 처리 중이면 무시
	if (bIsWipeoutInProgress) return;

	// 전멸 체크
	if (CheckTeamWipeout())
	{
		bIsWipeoutInProgress = true;

		// TODO: 전멸 UI 표시, 사운드 재생 등

		// 일정 시간 후 전멸 처리
		GetWorldTimerManager().SetTimer(
			WipeoutTimerHandle,
			this,
			&ADRGameModeBase::HandleWipeout,
			WipeoutDelayTime,
			false
		);
	}
}

bool ADRGameModeBase::CheckTeamWipeout()
{
	if (!HasAuthority()) return false;

	AGameStateBase* GameStateBase = GetGameState<AGameStateBase>();
	if (!GameStateBase) return false;

	int32 AlivePlayerCount = 0;
	int32 TotalPlayerCount = 0;

	// 모든 PlayerState 순회	
	for (APlayerState* PlayerState : GameStateBase->PlayerArray)
	{
		if (!PlayerState) continue;

		TotalPlayerCount++;

		// Character 가져오기
		ADRCharacterBase* Character = Cast<ADRCharacterBase>(PlayerState->GetPawn());
		if (!Character) continue;

		// CombatInterface로 죽음 체크
		if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Character))
		{
			if (!CombatInterface->Execute_IsDead(Character)) 
			{
				AlivePlayerCount++;
			}
		}
	}

	// 플레이어가 1명 이상 있고, 생존자가 0명이면 전멸
	return (TotalPlayerCount > 0) && (AlivePlayerCount == 0);
}

void ADRGameModeBase::HandleWipeout()
{
	if (!HasAuthority()) return;

	// 플래그 리셋
	bIsWipeoutInProgress = false;
}
