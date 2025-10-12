// Copyright DaeRune


#include "Game/DRLobbyGameState.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ADRLobbyGameState::ADRLobbyGameState()
{
	// 초기값 설정
	MinPlayersToStart = 1;  // 테스트용 1명, 실제는 2~4명 권장
}

void ADRLobbyGameState::BeginPlay()
{
	Super::BeginPlay();

	// 서버에서만 실행
	if (HasAuthority())
	{
		// 방 코드 초기화
		// 세션 서브시스템의 이벤트 구독
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UMultiplayerSessionsSubsystem* Subsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>())
			{
				// 이미 방 코드가 있으면 즉시 설정
				FString ExistingCode = Subsystem->GetCurrentRoomCode();
				if (!ExistingCode.IsEmpty())
				{
					SetRoomCode(ExistingCode);
				}
				else
				{
					// 없으면 이벤트 대기
					Subsystem->MultiplayerOnRoomCodeGenerated.AddDynamic(
						this,
						&ADRLobbyGameState::InitializeRoomCode
					);
				}
			}
		}
	}
}

void ADRLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRLobbyGameState, RoomCode);
}

void ADRLobbyGameState::SetRoomCode(const FString& NewRoomCode)
{
	if (HasAuthority())
	{
		RoomCode = NewRoomCode;
		OnRep_RoomCode();
	}
}

void ADRLobbyGameState::OnRep_RoomCode()
{
	OnRoomCodeGenerated.Broadcast(RoomCode);
}

void ADRLobbyGameState::InitializeRoomCode(const FString& NewRoomCode)
{
	SetRoomCode(NewRoomCode);
}
