// Copyright DaeRune


#include "Game/DRGameModeBase.h"
#include "Character/DRCharacterBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "Player/DRPlayerController.h"

ADRGameModeBase::ADRGameModeBase()
{
	WipeoutDelayTime = 3.0f;
	bIsWipeoutInProgress = false;
}

void ADRGameModeBase::Logout(AController* Exiting)
{
	// 접속 종료를 현재 페이즈에 먼저 통지한다 (Plan6 §5.10).
	// Super 호출 후에는 PlayerState 가 정리될 수 있으므로 순서가 중요하다.
	if (HasAuthority() && Exiting && Exiting->PlayerState)
	{
		NotifyPhasePlayerLeft(Exiting->PlayerState);
	}

	Super::Logout(Exiting);
}

void ADRGameModeBase::OnPlayerDied(APlayerState* DeadPlayer)
{
	if (!HasAuthority()) return;

	if (!DeadPlayer) return;

	// 현재 페이즈에 사망 통지 (Plan6 §14.3.5-A)
	// 전멸 처리와 무관하게 항상 전달한다.
	NotifyPhasePlayerDied(DeadPlayer);

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

