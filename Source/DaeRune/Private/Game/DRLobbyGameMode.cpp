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

	// 스테이지에서 돌아온 플레이어의 상태 리셋
	if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(NewPlayer))
	{
		// 관전 모드 강제 해제
		DRPC->ClientStopSpectating();

		// 입력 모드 리셋
		DRPC->SetInputMode(FInputModeGameOnly());
		DRPC->SetShowMouseCursor(false);

		// 플레이어 상태 플래그 리셋
		if (APawn* ControlledPawn = DRPC->GetPawn())
		{
			// Movement 컴포넌트 재활성화
			if (UCharacterMovementComponent* MovementComp = Cast<UCharacterMovementComponent>(ControlledPawn->GetMovementComponent()))
			{
				MovementComp->SetMovementMode(MOVE_Walking);
				MovementComp->SetComponentTickEnabled(true);
			}

			// 캡슐 콜리전 재활성화
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
	// 서버 체크
	if (!HasAuthority()) return;

	// 호스트 권한 체크
	if (!Requester || !Requester->IsLocalController()) return;

	// 맵 이름 유효성
	if (StageMapName.IsEmpty()) return;

	ExecuteTravel(StageMapName);
}

void ADRLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 로비는 도중 참가 허용!
	AllowJoinInProgress();
}

void ADRLobbyGameMode::HandleWipeout()
{
	if (!HasAuthority()) return;

	// TODO: 전멸 UI 표시 (로비는 가벼운 UI)

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
		// 로비에서는 도중 참가 허용!
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

		// PIE(Play In Editor) 프리픽스 제거
		// PIE에서는 "UEDPIE_0_MapName" 형식으로 나옴
		CurrentMapName.RemoveFromStart(World->StreamingLevelsPrefix);

		bUseSeamlessTravel = true;

		// 같은 맵 재시작
		World->ServerTravel(CurrentMapName + TEXT("?listen"));
	}

	// 플래그 리셋
	bIsWipeoutInProgress = false;
}

void ADRLobbyGameMode::ExecuteTravel(const FString& StageMapName)
{
	if (!HasAuthority()) return;

	CleanupAllAudioBeforeTravel();

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

void ADRLobbyGameMode::CleanupAllAudioBeforeTravel()
{
	if (!HasAuthority()) return;

	// 모든 클라이언트에게 오디오 정리 요청
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
		{
			PC->ClientStopAllAudio();
		}
	}

	// 약간의 대기 시간 (오디오 정리 완료 보장)
	FPlatformProcess::Sleep(0.1f);
}

