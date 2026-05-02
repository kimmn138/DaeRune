// Copyright DaeRune


#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Interfaces/VoiceInterface.h"

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete)),
	UpdateSessionCompleteDelegate(FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnUpdateSessionComplete)),
	SessionUserInviteAcceptedDelegate(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &ThisClass::OnSessionUserInviteAccepted))
{
}

void UMultiplayerSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		SessionInterface = Subsystem->GetSessionInterface();
		VoiceInterface = Subsystem->GetVoiceInterface();

		if (GEngine)
		{
			GEngine->OnNetworkFailure().AddUObject(this, &UMultiplayerSessionsSubsystem::HandleNetworkFailure);
		}

		// 占쏙옙占쏙옙 占십댐옙 占쏙옙占쏙옙占쏙옙 占쏙옙占?
		if (SessionInterface.IsValid())
		{
			SessionUserInviteAcceptedDelegateHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(SessionUserInviteAcceptedDelegate);
		}
	}
}

void UMultiplayerSessionsSubsystem::Deinitialize()
{
	Super::Deinitialize();

	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
	}

	SessionInterface = nullptr;
	VoiceInterface = nullptr;
}

void UMultiplayerSessionsSubsystem::CreateSessionWithRoomCode(int32 NumPublicConnections, const FString& MatchType)
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnCreateSessionComplete.Broadcast(false);
		return;
	}

	// 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		bCreateSessionOnDestroy = true;
		LastNumPublicConnections = NumPublicConnections;
		DestroySession();
		return;
	}

	// 占쏙옙 占쌘듸옙 占쏙옙占쏙옙占싹곤옙 占쌩븝옙 체크 占쏙옙占쏙옙
	bIsCreatingWithRoomCode = true;
	PendingRoomCode = GenerateRoomCode();
	LastNumPublicConnections = NumPublicConnections;

	ValidateAndCreateSessionWithCode();
}

void UMultiplayerSessionsSubsystem::FindSessionByRoomCode(const FString& RoomCode)
{
	if (!SessionInterface.IsValid() || RoomCode.IsEmpty())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	SearchingRoomCode = RoomCode;

	// 占쏙옙 占쌘듸옙占?占쏙옙占쏙옙 占싯삼옙
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = 100;
	LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;

	// 占쏙옙 占쌘듸옙占?占쏙옙占싶몌옙
	LastSessionSearch->QuerySettings.Set(FName("RoomCode"), RoomCode, EOnlineComparisonOp::Equals);
	LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		SearchingRoomCode.Empty();
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}

void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);

		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}
}

void UMultiplayerSessionsSubsystem::DestroySession()
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		MultiplayerOnDestroySessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::StartSession()
{
}

void UMultiplayerSessionsSubsystem::UpdateSessionJoinability(bool bAllowJoin)
{
	if (!SessionInterface.IsValid()) return;

	FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!ExistingSession) return;

	// 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙트
	ExistingSession->SessionSettings.bAllowJoinInProgress = bAllowJoin;

	// 占쏙옙占쏙옙占쏙옙占쏙옙트 占쏙옙占?
	UpdateSessionCompleteDelegateHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegate);

	// 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙트 占쏙옙占쏙옙
	if (!SessionInterface->UpdateSession(NAME_GameSession, ExistingSession->SessionSettings))
	{
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
	}
}

void UMultiplayerSessionsSubsystem::LeaveServer()
{
	if(!SessionInterface.IsValid()) return;

	UWorld* World = GetWorld();
	if(!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	StopVoiceChat();

	if (PC->HasAuthority())
	{
		if (DestroySessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
			DestroySessionCompleteDelegateHandle.Reset();
		}

		DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMultiplayerSessionsSubsystem::OnDestroySessionComplete));
		
		SessionInterface->DestroySession(NAME_GameSession);

		PC->ClientTravel(TEXT("/Game/Maps/MainMenu"), TRAVEL_Absolute);
	}
	else
	{
		PC->ClientTravel(TEXT("/Game/Maps/MainMenu"), TRAVEL_Absolute);

		SessionInterface->DestroySession(NAME_GameSession);
	}
}

void UMultiplayerSessionsSubsystem::StartVoiceChat()
{
	if(!VoiceInterface.IsValid()) return;

	VoiceInterface->RegisterLocalTalker(0);
	VoiceInterface->StartNetworkedVoice(0);
}

void UMultiplayerSessionsSubsystem::StopVoiceChat()
{
	if(!VoiceInterface.IsValid()) return;

	VoiceInterface->ClearVoicePackets();
	VoiceInterface->StopNetworkedVoice(0);
	VoiceInterface->RemoveAllRemoteTalkers();
	VoiceInterface->DisconnectAllEndpoints();
	VoiceInterface->UnregisterLocalTalker(0);

}

void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}

	if (bWasSuccessful && !CurrentRoomCode.IsEmpty())
	{
		MultiplayerOnRoomCodeGenerated.Broadcast(CurrentRoomCode);
	}
	else if (!bWasSuccessful)
	{
		CurrentRoomCode.Empty();
	}

	MultiplayerOnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}

	// 占쏙옙 占쌘듸옙 占쌩븝옙 체크 占쏙옙占싱억옙占쌕몌옙
	if (bIsCreatingWithRoomCode)
	{
		bIsCreatingWithRoomCode = false;

		if (LastSessionSearch.IsValid() && LastSessionSearch->SearchResults.Num() > 0)
		{
			// 占싱뱄옙 占쏙옙占쏙옙占싹댐옙 占쏙옙 占쌘듸옙占?占쏙옙占쏙옙 占쏙옙占쏙옙
			PendingRoomCode = GenerateRoomCode();
			ValidateAndCreateSessionWithCode();
		}
		else
		{
			// 占쌩븝옙 占쏙옙占쏙옙占쏙옙 占쏙옙 占쌘듸옙占?占쏙옙占쏙옙 占쏙옙占쏙옙
			CurrentRoomCode = PendingRoomCode;
			CreateSessionInternal(LastNumPublicConnections);
		}
		return;
	}

	// 占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占싯삼옙占싱억옙占쌕몌옙
	if (!SearchingRoomCode.IsEmpty())
	{
		SearchingRoomCode.Empty();
	}

	// 占싯삼옙 占쏙옙占?占쏙옙琯占식놂옙占싣?
	if (LastSessionSearch.IsValid())
	{
		MultiplayerOnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
	}
	else
	{
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}

	bInviteJoinStarted = false;
	bInvitePending = false;
	CachedInviteResult.Reset();

	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		// 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙
		if (SessionInterface.IsValid())
		{
			FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
			if (Session)
			{
				FString ExtractedRoomCode;
				Session->SessionSettings.Get(FName("RoomCode"), ExtractedRoomCode);

				if (!ExtractedRoomCode.IsEmpty())
				{
					CurrentRoomCode = ExtractedRoomCode;

					// 占쏙옙占쏙옙占쏙옙占쏙옙트 占쏙옙琯占식놂옙占싣? 占쏙옙占쏙옙 GameState占쏙옙 占쏙옙占쏙옙 占쏙옙 占쏙옙占쏙옙
					MultiplayerOnRoomCodeGenerated.Broadcast(CurrentRoomCode);
				}
			}
		}
	}

	MultiplayerOnJoinSessionComplete.Broadcast(Result);
}

void UMultiplayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}

	if (bWasSuccessful && bCreateSessionOnDestroy)
	{
		bCreateSessionOnDestroy = false;
		CreateSessionWithRoomCode(LastNumPublicConnections, "RoomCodeOnly");
	}

	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
}

void UMultiplayerSessionsSubsystem::OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
	}
}

void UMultiplayerSessionsSubsystem::OnSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
if (!bWasSuccessful) return;

	// 占싱뱄옙 占십대가 占쏙옙占?占쏙옙占싱거놂옙 Join 占쏙옙占쌜듸옙
	if (bInvitePending || bInviteJoinStarted) return;

	// 占십댐옙 占쏙옙占쏙옙 占쏙옙占쏙옙
	bInvitePending = true;
	CachedInviteResult = MakeShared<FOnlineSessionSearchResult>(InviteResult);

	// Join占쏙옙 占시듸옙占싹듸옙, 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占시울옙占쏙옙 占쏙옙占쏙옙占?
	TryProcessPendingInvite();
}

void UMultiplayerSessionsSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占?占쏙옙占싱쏙옙 채占시듸옙 占쏙옙占쏙옙
	StopVoiceChat();

	SessionInterface->DestroySession(NAME_GameSession);

	if (FailureType == ENetworkFailure::Type::ConnectionLost || FailureType == ENetworkFailure::Type::FailureReceived)
	{
		if (World && World->GetFirstPlayerController())
		{
			// NOTE: crash 占쌩삼옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 Travel 占쌍쇽옙처占쏙옙. (占썩본 占쏙옙占쏙옙占쏙옙 占싱듸옙占싹니깍옙 占쌓놂옙 占쏙옙占쏙옙占쏙옙)
			// World->GetFirstPlayerController()->ClientTravel(TEXT("/Game/VoiceChat/MainMenu"), TRAVEL_Absolute);
		}
	}
}

FString UMultiplayerSessionsSubsystem::GenerateRoomCode()
{
	const FString CharacterPool = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	FString RoomCode;
	RoomCode.Reserve(ROOM_CODE_LENGTH);

	for (int32 i = 0; i < ROOM_CODE_LENGTH; i++)
	{
		int32 RandomIndex = FMath::RandRange(0, CharacterPool.Len() - 1);
		RoomCode.AppendChar(CharacterPool[RandomIndex]);
	}

	return RoomCode;
}

void UMultiplayerSessionsSubsystem::ValidateAndCreateSessionWithCode()
{
	if (!SessionInterface.IsValid())
	{
		return;
	}

	bIsCreatingWithRoomCode = true;

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = 100;
	LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	LastSessionSearch->QuerySettings.Set(FName("RoomCode"), PendingRoomCode, EOnlineComparisonOp::Equals);
	LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		bIsCreatingWithRoomCode = false;
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}

void UMultiplayerSessionsSubsystem::CreateSessionInternal(int32 NumPublicConnections)
{
	if (!SessionInterface.IsValid())
	{
		return;
	}

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	LastSessionSettings->bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	LastSessionSettings->NumPublicConnections = NumPublicConnections;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	LastSessionSettings->BuildUniqueId = 1;

	// 占쏙옙 占쌘드를 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쌩곤옙
	LastSessionSettings->Set(FName("RoomCode"), CurrentRoomCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CurrentRoomCode.Empty();
		MultiplayerOnCreateSessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::TryProcessPendingInvite()
{
	if (!bInvitePending || bInviteJoinStarted || !CachedInviteResult.IsValid()) return;

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance) return;

	UWorld* World = GameInstance->GetWorld();
	if (!World) return;

	if (World->GetFirstLocalPlayerFromController() == nullptr) return;

	if (!SessionInterface.IsValid()) return;

	bInviteJoinStarted = true;
	bInvitePending = false;

	CachedInviteResult->Session.SessionSettings.bUsesPresence = true;
	CachedInviteResult->Session.SessionSettings.bUseLobbiesIfAvailable = true;

	JoinSession(*CachedInviteResult);
	CachedInviteResult.Reset();
}

