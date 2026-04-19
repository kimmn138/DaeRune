// Copyright DaeRune


#include "Game/DRLobbyGameState.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ADRLobbyGameState::ADRLobbyGameState()
{
	// �ʱⰪ ����
	MinPlayersToStart = 1;  // �׽�Ʈ�� 1��, ������ 2~4�� ����
}

void ADRLobbyGameState::BeginPlay()
{
	Super::BeginPlay();

	// ���������� ����
	if (HasAuthority())
	{
		// �� �ڵ� �ʱ�ȭ
		// ���� ����ý����� �̺�Ʈ ����
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UMultiplayerSessionsSubsystem* Subsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>())
			{
				// �̹� �� �ڵ尡 ������ ��� ����
				FString ExistingCode = Subsystem->GetCurrentRoomCode();
				if (!ExistingCode.IsEmpty())
				{
					SetRoomCode(ExistingCode);
				}
				else
				{
					// ������ �̺�Ʈ ���
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
	DOREPLIFETIME(ADRLobbyGameState, LobbyState);
}

void ADRLobbyGameState::SetRoomCode(const FString& NewRoomCode)
{
	if (HasAuthority())
	{
		RoomCode = NewRoomCode;
		// 서버에서 직접 브로드캐스트 (OnRep은 서버에서 호출되지 않음)
		BroadcastRoomCode();
	}
}

void ADRLobbyGameState::OnRep_RoomCode()
{
	// 클라이언트에서 복제 시 브로드캐스트
	BroadcastRoomCode();
}

void ADRLobbyGameState::BroadcastRoomCode()
{
	OnRoomCodeGenerated.Broadcast(RoomCode);
}

void ADRLobbyGameState::InitializeRoomCode(const FString& NewRoomCode)
{
	SetRoomCode(NewRoomCode);
}

void ADRLobbyGameState::SetLobbyState(ELobbyState NewState)
{
	if (!HasAuthority()) return;
	if (LobbyState == NewState) return;

	LobbyState = NewState;
	// 서버에서 직접 브로드캐스트 (OnRep은 클라이언트에서만 호출됨)
	OnLobbyStateChanged.Broadcast(LobbyState);
}

void ADRLobbyGameState::OnRep_LobbyState()
{
	OnLobbyStateChanged.Broadcast(LobbyState);
}
