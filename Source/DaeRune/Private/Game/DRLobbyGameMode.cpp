// Copyright DaeRune


#include "Game/DRLobbyGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Player/DRPlayerController.h"
#include "MultiplayerSessionsSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

ADRLobbyGameMode::ADRLobbyGameMode()
{
	WipeoutDelayTime = 2.0f;
}

void ADRLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 스테이지에서 돌아온 플레이어의 상태 복원
	if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(NewPlayer))
	{
		// 관전 모드 강제 종료
		DRPC->ClientStopSpectating();

		// 입력 모드는 PlayerController::ReceivedPlayer()에서 RestoreDefaultInputMode()로 처리됨

		// 플레이어 상태 플래그 복원
		if (APawn* ControlledPawn = DRPC->GetPawn())
		{
			// Movement ������Ʈ ��Ȱ��ȭ
			if (UCharacterMovementComponent* MovementComp = Cast<UCharacterMovementComponent>(ControlledPawn->GetMovementComponent()))
			{
				MovementComp->SetMovementMode(MOVE_Walking);
				MovementComp->SetComponentTickEnabled(true);
			}

			// ĸ�� �ݸ��� ��Ȱ��ȭ
			if (UCapsuleComponent* CapsuleComp = Cast<UCapsuleComponent>(ControlledPawn->GetRootComponent()))
			{
				CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
		}
	}

	if (GameState)
	{
		int32 NumberOfPlayers = GameState.Get()->PlayerArray.Num();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				1,
				600.f,
				FColor::Yellow,
				FString::Printf(TEXT("Players in game: %d"), NumberOfPlayers)
			);

			APlayerState* PlayerState = NewPlayer->GetPlayerState<APlayerState>();
			if (PlayerState)
			{
				FString PlayerName = PlayerState->GetPlayerName();
				GEngine->AddOnScreenDebugMessage(
					-1,
					60.f,
					FColor::Cyan,
					FString::Printf(TEXT("%s has joined the game!"), *PlayerName)
				);
			}
		}
	}
}

void ADRLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	APlayerState* PlayerState = Exiting->GetPlayerState<APlayerState>();
	if (PlayerState)
	{
		int32 NumberOfPlayers = GameState.Get()->PlayerArray.Num();
		GEngine->AddOnScreenDebugMessage(
			1,
			600.f,
			FColor::Yellow,
			FString::Printf(TEXT("Players in game: %d"), NumberOfPlayers - 1)
		);

		FString PlayerName = PlayerState->GetPlayerName();
		GEngine->AddOnScreenDebugMessage(
			-1,
			60.f,
			FColor::Cyan,
			FString::Printf(TEXT("%s has exited the game!"), *PlayerName)
		);
	}
}

void ADRLobbyGameMode::TravelToStage(const FString& StageMapName, ADRPlayerController* Requester)
{
	// ���� üũ
	if (!HasAuthority()) return;

	// ȣ��Ʈ ���� üũ
	if (!Requester || !Requester->IsLocalController()) return;

	// �� �̸� ��ȿ��
	if (StageMapName.IsEmpty()) return;

	ExecuteTravel(StageMapName);
}

void ADRLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	// �κ�� ���� ���� ���!
	AllowJoinInProgress();
}

void ADRLobbyGameMode::HandleWipeout()
{
	if (!HasAuthority()) return;

	// TODO: ���� UI ǥ�� (�κ�� ������ UI)

	RestartLobby();
}

void ADRLobbyGameMode::AllowJoinInProgress()
{
	if (!HasAuthority()) return;

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance) return;

	UMultiplayerSessionsSubsystem* SessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	if (SessionsSubsystem)
	{
		// �κ񿡼��� ���� ���� ���!
		SessionsSubsystem->UpdateSessionJoinability(true);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
				TEXT("Lobby loaded - Join in progress ALLOWED!"));
		}
	}
}

void ADRLobbyGameMode::RestartLobby()
{
	if (!HasAuthority()) return;

	UWorld* World = GetWorld();
	if (World)
	{
		FString CurrentMapName = World->GetMapName();

		// PIE(Play In Editor) �����Ƚ� ����
		// PIE������ "UEDPIE_0_MapName" �������� ����
		CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);

		bUseSeamlessTravel = true;

		// ���� �� �����
		World->ServerTravel(CurrentMapName + TEXT("?listen"));
	}

	// �÷��� ����
	bIsWipeoutInProgress = false;
}

void ADRLobbyGameMode::ExecuteTravel(const FString& StageMapName)
{
	if (!HasAuthority()) return;

	// 맵 전환 전 정리 작업 (부모 클래스의 공통 함수 사용)
	PrepareForTravel();

	UWorld* World = GetWorld();
	if (World)
	{
		bUseSeamlessTravel = true;
		// URL 구성
		FString TravelURL = StageMapName;
		if (!TravelURL.Contains(TEXT("?")))
		{
			TravelURL += TEXT("?listen");
		}

		// 맵 이동
		World->ServerTravel(TravelURL);
	}
}

