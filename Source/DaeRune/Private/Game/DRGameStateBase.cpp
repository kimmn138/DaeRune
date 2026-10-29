// Copyright DaeRune


#include "Game/DRGameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Character/DRCharacter.h"
#include "Interaction/CombatInterface.h"
#include "Player/DRPlayerState.h"

ADRGameStateBase::ADRGameStateBase()
{
	// 리플리케이션 활성화
	bReplicates = true;
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

TArray<ADRCharacter*> ADRGameStateBase::GetAlivePlayers() const
{
	TArray<ADRCharacter*> AlivePlayers;

	// 모든 PlayerState 순회
	for (APlayerState* PS : PlayerArray)
	{
		if (!PS) continue;

		// 플레이어의 폰이 살아있는지 확인
		ADRCharacter* PlayerCharacter = Cast<ADRCharacter>(PS->GetPawn());
		if (PlayerCharacter && !ICombatInterface::Execute_IsDead(PlayerCharacter))
		{
			AlivePlayers.Add(PlayerCharacter);
		}
	}

	return AlivePlayers;
}
