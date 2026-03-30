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

	// �̹� ���� ó�� ���̸� ����
	if (bIsWipeoutInProgress) return;

	// ���� üũ
	if (CheckTeamWipeout())
	{
		bIsWipeoutInProgress = true;

		// ��� �÷��̾�� ���� ���� UI ǥ��
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
			{
				PC->Client_ShowGameOverUI();
			}
		}

		// ���� �ð� �� ���� ó��
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

	// ��� PlayerState ��ȸ	
	for (APlayerState* PlayerState : GameStateBase->PlayerArray)
	{
		if (!PlayerState) continue;

		TotalPlayerCount++;

		// Character ��������
		ADRCharacterBase* Character = Cast<ADRCharacterBase>(PlayerState->GetPawn());
		if (!Character) continue;

		// CombatInterface�� ���� üũ
		if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Character))
		{
			if (!CombatInterface->Execute_IsDead(Character)) 
			{
				AlivePlayerCount++;
			}
		}
	}

	// �÷��̾ 1�� �̻� �ְ�, �����ڰ� 0���̸� ����
	return (TotalPlayerCount > 0) && (AlivePlayerCount == 0);
}

void ADRGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	// Ž�� �Ŵ��� ����
	SpawnDetectionManager();
}

void ADRGameModeBase::HandleWipeout()
{
	if (!HasAuthority()) return;

	// 플래그 리셋
	bIsWipeoutInProgress = false;
}

void ADRGameModeBase::PrepareForTravel()
{
	if (!HasAuthority()) return;

	// 모든 클라이언트에게 설정창 닫기 및 오디오 정리 요청
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
		{
			PC->ClientCloseSettingsMenu();
			PC->ClientStopAllAudio();
		}
	}
}

void ADRGameModeBase::SpawnDetectionManager()
{
	// ���������� ����
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
