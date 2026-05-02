// Copyright DaeRune


#include "Menu.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"

void UMenu::MenuSetup(int32 NumberOfPublicConnections, const FString& LobbyPath)
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
		UWorld* World = GetWorld();
		if (World)
		{
			World->ServerTravel(PathToLobby);
		}
	}
	else
	{
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

	// 占쏙옙 占쌘듸옙罐占?占쏙옙치占쏙옙占쏙옙킹
	bool bFoundRoom = false;

	if (bWasSuccessful && SessionResults.Num() > 0)
	{
		for (const auto& Result : SessionResults)
		{
			FString FoundRoomCode;
			Result.Session.SessionSettings.Get(FName("RoomCode"), FoundRoomCode);

			// 占쌉뤄옙占쏙옙 占쏙옙 占쌘듸옙占?占쏙옙치占싹댐옙占쏙옙 확占쏙옙
			if (FoundRoomCode == PendingJoinRoomCode)
			{
				// const 李몄“?대?濡?蹂듭궗蹂??앹꽦 ???섏젙
				FOnlineSessionSearchResult ModifiableResult = Result;
				ModifiableResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;
				ModifiableResult.Session.SessionSettings.bUsesPresence = true;
				MultiplayerSessionsSubsystem->JoinSession(ModifiableResult);
				bFoundRoom = true;
				return;
			}
		}
	}

	if (!bFoundRoom)
	{
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
	if (!RoomCodeInputBox || !JoinButton)
	{
		return;
	}

	FString InputRoomCode = RoomCodeInputBox->GetText().ToString().ToUpper();

	if (InputRoomCode.Len() != ROOM_CODE_LENGTH)
	{
		return;
	}

	// 占쏙옙占식븝옙占쏙옙 占쏙옙占쌘몌옙 占쏙옙占?
	for (TCHAR Char : InputRoomCode)
	{
		if (!FChar::IsAlnum(Char))
		{
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

