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

		// 스팀 초대 리스너 등록
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

	// 기존 세션 있으면 먼저 삭제
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		bCreateSessionOnDestroy = true;
		LastNumPublicConnections = NumPublicConnections;
		DestroySession();
		return;
	}

	// 방 코드 생성하고 중복 체크 시작
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

	// 방 코드로 세션 검색
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = 100;
	LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;

	// 방 코드로 필터링
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

	// 세션 설정 업데이트
	ExistingSession->SessionSettings.bAllowJoinInProgress = bAllowJoin;

	// 델리게이트 등록
	UpdateSessionCompleteDelegateHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegate);

	// 세션 업데이트 실행
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

	// 방 코드 중복 체크 중이었다면
	if (bIsCreatingWithRoomCode)
	{
		bIsCreatingWithRoomCode = false;

		if (LastSessionSearch.IsValid() && LastSessionSearch->SearchResults.Num() > 0)
		{
			// 이미 존재하는 방 코드면 새로 생성
			PendingRoomCode = GenerateRoomCode();
			ValidateAndCreateSessionWithCode();
		}
		else
		{
			// 중복 없으면 이 코드로 세션 생성
			CurrentRoomCode = PendingRoomCode;
			CreateSessionInternal(LastNumPublicConnections);
		}
		return;
	}

	// 방 참가를 위한 검색이었다면
	if (!SearchingRoomCode.IsEmpty())
	{
		SearchingRoomCode.Empty();
	}

	// 검색 결과 브로드캐스트
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
		// 세션 정보 가져오기
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

					// 델리게이트 브로드캐스트! 이제 GameState가 받을 수 있음
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
	UE_LOG(LogTemp, Log, TEXT("[Invite] Received"));

	if (!bWasSuccessful) return;

	// 이미 초대가 대기 중이거나 Join 시작됨
	if (bInvitePending || bInviteJoinStarted) return;

	// 초대 정보 저장
	bInvitePending = true;
	CachedInviteResult = MakeShared<FOnlineSessionSearchResult>(InviteResult);

	// Join을 시도하되, 성공 조건 만족 시에만 진행됨
	TryProcessPendingInvite();
}

void UMultiplayerSessionsSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// 연결이 끊기면 보이스 채팅도 중지
	StopVoiceChat();

	SessionInterface->DestroySession(NAME_GameSession);

	if (FailureType == ENetworkFailure::Type::ConnectionLost || FailureType == ENetworkFailure::Type::FailureReceived)
	{
		if (World && World->GetFirstPlayerController())
		{
			// NOTE: crash 발생으로 인해 명시적 Travel 주석처리. (기본 레벨로 이동하니까 그냥 냅두자)
			// World->GetFirstPlayerController()->ClientTravel(TEXT("/Game/VoiceChat/MainMenu"), TRAVEL_Absolute);
		}
	}
}

FString UMultiplayerSessionsSubsystem::GenerateRoomCode()
{
	// A~Z, 0~9 문자 풀
	const FString CharacterPool = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	FString RoomCode;

	// 8글자 랜덤 생성
	for (int32 i = 0; i < 8; i++)
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

	// 방 코드를 세션 설정에 추가
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
