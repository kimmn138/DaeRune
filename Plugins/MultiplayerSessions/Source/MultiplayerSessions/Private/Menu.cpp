// Copyright DaeRune


#include "Menu.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"

void UMenu::MenuSetup(int32 NumberOfPublicConnections, FString LobbyPath)
{
	PathToLobby = FString::Printf(TEXT("%s?listen"), *LobbyPath);
	NumPublicConnections = NumberOfPublicConnections;
	
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);

	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	}

	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnCreateSession);
		MultiplayerSessionsSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessions);
		MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &ThisClass::OnDestroySession);
		MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &ThisClass::OnStartSession);
	}
}

bool UMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &ThisClass::HostButtonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::JoinButtonClicked);
	}

	return true;
}

void UMenu::NativeDestruct()
{
	MenuTearDown();

	Super::NativeDestruct();
}

void UMenu::OnCreateSession(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Yellow,
				FString::Printf(TEXT("Session created successfully!"))
			);
		}

		UWorld* World = GetWorld();
		if (World)
		{
			World->ServerTravel(PathToLobby);
		}
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Red,
				FString::Printf(TEXT("Failed to create session!"))
			);
		}
		HostButton->SetIsEnabled(true);
	}
}

void UMenu::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		JoinButton->SetIsEnabled(true);
		return;
	}

	// 방 코드로만 매치메이킹
	bool bFoundRoom = false;

	if (bWasSuccessful && SessionResults.Num() > 0)
	{
		for (auto Result : SessionResults)
		{
			FString FoundRoomCode;
			Result.Session.SessionSettings.Get(FName("RoomCode"), FoundRoomCode);

			// 입력한 방 코드와 일치하는지 확인
			if (FoundRoomCode == PendingJoinRoomCode)
			{
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(
						-1,
						5.f,
						FColor::Green,
						FString::Printf(TEXT("[Menu] MATCH! Joining session with code: %s"), *FoundRoomCode)
					);
				}
				Result.Session.SessionSettings.bUseLobbiesIfAvailable = true;
				Result.Session.SessionSettings.bUsesPresence = true;
				MultiplayerSessionsSubsystem->JoinSession(Result);
				bFoundRoom = true;
				return;
			}
		}
	}

	if (!bFoundRoom)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				5.f,
				FColor::Red,
				FString::Printf(TEXT("No room found with code: %s"), *PendingJoinRoomCode)
			);
		}

		JoinButton->SetIsEnabled(true);
		PendingJoinRoomCode.Empty();
	}
}

void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
		if (Subsystem)
		{
			IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
			if (SessionInterface.IsValid())
			{
				FString Address;
				SessionInterface->GetResolvedConnectString(NAME_GameSession, Address);

				APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
				if (PlayerController)
				{
					PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
				}
			}
		}

		PendingJoinRoomCode.Empty();
	}
	else
	{
		JoinButton->SetIsEnabled(true);
		PendingJoinRoomCode.Empty();
	}
}

void UMenu::OnDestroySession(bool bWasSuccessful)
{
}

void UMenu::OnStartSession(bool bWasSuccessful)
{
}

void UMenu::HostButtonClicked()
{
	HostButton->SetIsEnabled(false);
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->CreateSessionWithRoomCode(NumPublicConnections, FString("RoomCodeOnly"));
	}
}

void UMenu::JoinButtonClicked()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.f,
			FColor::Cyan,
			TEXT("[Menu] JoinButtonClicked() called!")
		);
	}

	if (!RoomCodeInputBox || !JoinButton)
	{
		return;
	}

	FString InputRoomCode = RoomCodeInputBox->GetText().ToString().ToUpper();

	// 8글자 체크
	if (InputRoomCode.Len() != 8)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				5.f,
				FColor::Red,
				FString::Printf(TEXT("Room code must be 8 characters!"))
			);
		}
		return;
	}

	// 알파벳과 숫자만 허용
	for (TCHAR Char : InputRoomCode)
	{
		if (!FChar::IsAlnum(Char))
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					-1,
					5.f,
					FColor::Red,
					TEXT("Room code must contain only letters and numbers!")
				);
			}
			return;
		}
	}

	JoinButton->SetIsEnabled(false);
	PendingJoinRoomCode = InputRoomCode;

	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->FindSessionByRoomCode(InputRoomCode);
	}
}

void UMenu::MenuTearDown()
{
	RemoveFromParent();
}
