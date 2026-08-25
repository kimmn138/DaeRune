// Copyright DaeRune


#include "Game/DRGameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework//PlayerState.h"
#include "Engine/World.h"
#include "Character/DRCharacter.h"
#include "Interaction/CombatInterface.h"
#include "Player/DRPlayerState.h"

ADRGameStateBase::ADRGameStateBase()
{
	// ���ø����̼� Ȱ��ȭ
	bReplicates = true;

	// �ʱⰪ ����
	CurrentPlayerCount = 0;
	MaxPlayerCount = 4;  // 4�� ���� ����
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
		// 리슨 서버에서 로컬 컨트롤러 소유의 PlayerState가 호스트
		if (ADRPlayerState* DRPS = Cast<ADRPlayerState>(PlayerState))
		{
			const APlayerController* PC = Cast<APlayerController>(DRPS->GetOwner());
			if (PC && PC->IsLocalController())
			{
				DRPS->SetIsHost(true);
			}
		}

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
	// 서버가 접속 시 확정해 복제한 bIsHost 플래그로 판별
	for (APlayerState* PS : PlayerArray)
	{
		const ADRPlayerState* DRPS = Cast<ADRPlayerState>(PS);
		if (DRPS && DRPS->IsHost())
		{
			return PS;
		}
	}

	// �� ã���� ù ��° �÷��̾ ȣ��Ʈ��
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

TArray<ADRCharacter*> ADRGameStateBase::GetAlivePlayers() const
{
	TArray<ADRCharacter*> AlivePlayers;

	// 모든 PlayerState 순회
	for (APlayerState* PS : PlayerArray)
	{
		if (!PS) continue;

		// 플레이어의 폰이 살아있는지 확인
		if (ADRCharacter* PlayerCharacter = Cast<ADRCharacter>(PS->GetPawn()))
		{
			// IsDead 인터페이스 확인
			if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(PlayerCharacter))
			{
				if (!CombatInterface->Execute_IsDead(PlayerCharacter))
				{
					AlivePlayers.Add(PlayerCharacter);
				}
			}
		}
	}

	return AlivePlayers;
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
