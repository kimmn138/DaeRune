// Copyright DaeRune


#include "Game/DRGameModeBase.h"
#include "Character/DRCharacterBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "Game/DRDetectionManager.h"
#include "Player/DRPlayerController.h"

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

		// 모든 플레이어에게 게임 오버 UI 표시
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
			{
				PC->Client_ShowGameOverUI();
			}
		}

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

void ADRGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	// 탐지 매니저 스폰
	SpawnDetectionManager();
}

void ADRGameModeBase::HandleWipeout()
{
	if (!HasAuthority()) return;

	// 플래그 리셋
	bIsWipeoutInProgress = false;
}

void ADRGameModeBase::SpawnDetectionManager()
{
	// 서버에서만 스폰
	if (!HasAuthority()) return;

	if (!DetectionManagerClass) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	DetectionManager = GetWorld()->SpawnActor<ADRDetectionManager>(
		DetectionManagerClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams
	);
}
