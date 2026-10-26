// Copyright DaeRune


#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Interfaces/VoiceInterface.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	const FName RoomCodeKey(TEXT("RoomCode"));
}

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete)),
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

		// 스팀 초대 수락 (게임 실행 중 / 초대로 실행된 직후 모두 이 경로로 들어온다)
		if (SessionInterface.IsValid())
		{
			SessionUserInviteAcceptedDelegateHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(SessionUserInviteAcceptedDelegate);
		}
	}

	// 초대를 받았을 때 로컬 플레이어가 아직 없었다면(맵 로딩 중) 맵 로드가 끝난 뒤 다시 시도한다
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMultiplayerSessionsSubsystem::HandlePostLoadMap);
}

void UMultiplayerSessionsSubsystem::Deinitialize()
{
	Super::Deinitialize();

	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
	}

	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	// Initialize에서 등록한 초대 수락 델리게이트 해제
	if (SessionInterface.IsValid() && SessionUserInviteAcceptedDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionUserInviteAcceptedDelegateHandle);
		SessionUserInviteAcceptedDelegateHandle.Reset();
	}

	SessionInterface = nullptr;
	VoiceInterface = nullptr;
}

bool UMultiplayerSessionsSubsystem::IsLanSubsystem() const
{
	const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	return Subsystem && Subsystem->GetSubsystemName() == TEXT("NULL");
}

const ULocalPlayer* UMultiplayerSessionsSubsystem::GetSessionLocalPlayer() const
{
	// 레벨 전환 타이밍에는 월드나 로컬 플레이어가 없을 수 있다
	const UWorld* World = GetWorld();
	return World ? World->GetFirstLocalPlayerFromController() : nullptr;
}

TSharedRef<FOnlineSessionSearch> UMultiplayerSessionsSubsystem::MakeRoomCodeSearch(const FString& RoomCode)
{
	TSharedRef<FOnlineSessionSearch> Search = MakeShared<FOnlineSessionSearch>();
	Search->MaxSearchResults = 100;
	Search->bIsLanQuery = IsLanSubsystem();
	Search->QuerySettings.Set(RoomCodeKey, RoomCode, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	LastSessionSearch = Search;
	return Search;
}

void UMultiplayerSessionsSubsystem::TravelToMainMenu(APlayerController* PC) const
{
	if (PC)
	{
		PC->ClientTravel(MainMenuMapPath, TRAVEL_Absolute);
	}
}

void UMultiplayerSessionsSubsystem::CreateSessionWithRoomCode(int32 NumPublicConnections)
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnCreateSessionComplete.Broadcast(false);
		return;
	}

	// 이전 세션이 남아 있으면 정리한 뒤 다시 만든다 (AlreadyInSession 실패 방지)
	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		bCreateSessionOnDestroy = true;
		LastNumPublicConnections = NumPublicConnections;
		DestroySession();
		return;
	}

	// 룸 코드를 만들고 같은 코드의 방이 이미 있는지 검사한 뒤 생성한다
	PendingRoomCode = GenerateRoomCode();
	LastNumPublicConnections = NumPublicConnections;
	RoomCodeRetryCount = 0;

	ValidateAndCreateSessionWithCode();
}

void UMultiplayerSessionsSubsystem::FindSessionByRoomCode(const FString& RoomCode)
{
	if (!SessionInterface.IsValid() || RoomCode.IsEmpty())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	// 기존 세션이 남아있으면 먼저 정리 후 같은 룸코드로 재시도 (AlreadyInSession 실패 방지)
	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		bFindSessionOnDestroy = true;
		PendingFindRoomCode = RoomCode;
		DestroySession();
		return;
	}

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	const TSharedRef<FOnlineSessionSearch> Search = MakeRoomCodeSearch(RoomCode);
	const ULocalPlayer* LocalPlayer = GetSessionLocalPlayer();
	if (!LocalPlayer || !SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), Search))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
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

	// 기존 세션이 남아있으면 먼저 정리 후 같은 결과로 재시도 (AlreadyInSession 실패 방지)
	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		bJoinSessionOnDestroy = true;
		PendingJoinResult = MakeShared<FOnlineSessionSearchResult>(SessionResult);
		DestroySession();
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	const ULocalPlayer* LocalPlayer = GetSessionLocalPlayer();
	if (!LocalPlayer || !SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);

		// 완료 콜백이 오지 않으므로 초대 진행 표시도 여기서 푼다 (안 풀면 이후 초대를 영영 무시한다)
		bInviteJoinStarted = false;
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

void UMultiplayerSessionsSubsystem::UpdateSessionJoinability(bool bAllowJoin)
{
	if (!SessionInterface.IsValid()) return;

	FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!ExistingSession) return;

	// 진행 중 참가 허용 여부만 바꿔서 세션 설정을 갱신한다
	ExistingSession->SessionSettings.bAllowJoinInProgress = bAllowJoin;

	UpdateSessionCompleteDelegateHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegate);

	if (!SessionInterface->UpdateSession(NAME_GameSession, ExistingSession->SessionSettings))
	{
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
	}
}

void UMultiplayerSessionsSubsystem::LeaveServer()
{
	UWorld* World = GetWorld();
	if(!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	StopVoiceChat();

	// 정리할 세션이 없으면 바로 메인메뉴로 이동
	if (!SessionInterface.IsValid() || SessionInterface->GetNamedSession(NAME_GameSession) == nullptr)
	{
		TravelToMainMenu(PC);
		return;
	}

	// 세션 파괴 완료 콜백에서 메인메뉴로 이동 (호스트/클라 공통)
	// 즉시 Travel하면 파괴 콜백이 떠나는 월드 기준으로 발화할 수 있어 콜백 기반으로 순서 표준화
	if (DestroySessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();
	}

	bTravelToMainMenuOnDestroy = true;
	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		// 파괴 시작 자체가 실패하면 콜백이 오지 않으므로 즉시 이동
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();
		bTravelToMainMenuOnDestroy = false;
		TravelToMainMenu(PC);
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

	// 방 생성 전 룸 코드 중복 검사였다면 생성 흐름으로 돌아간다
	if (bIsCreatingWithRoomCode)
	{
		bIsCreatingWithRoomCode = false;

		if (LastSessionSearch.IsValid() && LastSessionSearch->SearchResults.Num() > 0)
		{
			// 중복 룸코드 발견 - 새 코드를 생성해 재검사 (상한 초과 시 실패 브로드캐스트로 무한 루프 탈출)
			if (++RoomCodeRetryCount >= MaxRoomCodeRetries)
			{
				RoomCodeRetryCount = 0;
				CurrentRoomCode.Empty();
				MultiplayerOnCreateSessionComplete.Broadcast(false);
				return;
			}

			PendingRoomCode = GenerateRoomCode();
			ValidateAndCreateSessionWithCode();
		}
		else
		{
			// 중복 없음 - 이 룸코드로 세션 생성
			RoomCodeRetryCount = 0;
			CurrentRoomCode = PendingRoomCode;
			CreateSessionInternal(LastNumPublicConnections);
		}
		return;
	}

	// 참가용 검색 결과를 메뉴에 넘긴다
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

	if (Result == EOnJoinSessionCompleteResult::Success && SessionInterface.IsValid())
	{
		// 참가한 세션의 룸 코드를 꺼내 둔다 (로비 GameState 가 이 코드를 표시한다)
		if (FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession))
		{
			FString ExtractedRoomCode;
			Session->SessionSettings.Get(RoomCodeKey, ExtractedRoomCode);

			if (!ExtractedRoomCode.IsEmpty())
			{
				CurrentRoomCode = ExtractedRoomCode;
				MultiplayerOnRoomCodeGenerated.Broadcast(CurrentRoomCode);
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

	if (bWasSuccessful)
	{
		if (bCreateSessionOnDestroy)
		{
			bCreateSessionOnDestroy = false;
			CreateSessionWithRoomCode(LastNumPublicConnections);
		}
		else if (bFindSessionOnDestroy)
		{
			bFindSessionOnDestroy = false;
			const FString RoomCode = PendingFindRoomCode;
			PendingFindRoomCode.Empty();
			FindSessionByRoomCode(RoomCode);
		}
		else if (bJoinSessionOnDestroy)
		{
			bJoinSessionOnDestroy = false;
			if (PendingJoinResult.IsValid())
			{
				const FOnlineSessionSearchResult Result = *PendingJoinResult;
				PendingJoinResult.Reset();
				JoinSession(Result);
			}
		}
	}
	else
	{
		// Destroy 실패 시 보류 상태 정리하여 데드락 방지
		bCreateSessionOnDestroy = false;
		bFindSessionOnDestroy = false;
		bJoinSessionOnDestroy = false;
		PendingFindRoomCode.Empty();
		PendingJoinResult.Reset();
	}

	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);

	// LeaveServer 경로: 세션 정리가 끝난 뒤 메인메뉴로 이동 (성공/실패 무관 - 어차피 떠나는 중)
	if (bTravelToMainMenuOnDestroy)
	{
		bTravelToMainMenuOnDestroy = false;
		if (UWorld* World = GetWorld())
		{
			TravelToMainMenu(World->GetFirstPlayerController());
		}
	}
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

	// 이미 처리 중인 초대가 있으면 무시한다 (중복 Join 방지)
	if (bInvitePending || bInviteJoinStarted) return;

	bInvitePending = true;
	CachedInviteResult = MakeShared<FOnlineSessionSearchResult>(InviteResult);

	// 지금 바로 참가할 수 없으면(로컬 플레이어 없음) 맵 로드 후 HandlePostLoadMap 이 다시 시도한다
	TryProcessPendingInvite();
}

void UMultiplayerSessionsSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// 델리게이트가 전역이라(PIE 한 프로세스에 여러 인스턴스) 이 GameInstance 의 월드만 처리한다
	if (LoadedWorld && LoadedWorld->GetGameInstance() == GetGameInstance())
	{
		TryProcessPendingInvite();
	}
}

void UMultiplayerSessionsSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// ★리슨 서버 호스트에서는 무시한다★
	// 엔진은 클라이언트 한 명의 연결 타임아웃(UNetConnection::HandleConnectionTimeout)에도 서버 쪽에서 이 신호를 보낸다.
	// 여기서 세션을 파괴하면 게임 도중 호스트의 로비가 사라져 이후 초대·재접속이 막힌다.
	if (NetDriver && NetDriver->IsServer())
	{
		return;
	}

	// 다른 GameInstance(PIE 다른 인스턴스)의 실패는 이 서브시스템과 무관하다
	if (World && World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	// 서버와의 연결을 잃은 클라이언트: 보이스 채팅과 세션을 정리한다
	StopVoiceChat();

	// OnlineSubsystem이 없으면 SessionInterface가 설정되지 않은 채로 이 델리게이트가 발화할 수 있음
	if (SessionInterface.IsValid())
	{
		SessionInterface->DestroySession(NAME_GameSession);
	}
}

FString UMultiplayerSessionsSubsystem::GenerateRoomCode()
{
	const FString CharacterPool = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	FString RoomCode;
	RoomCode.Reserve(ROOM_CODE_LENGTH);

	for (int32 i = 0; i < ROOM_CODE_LENGTH; i++)
	{
		const int32 RandomIndex = FMath::RandRange(0, CharacterPool.Len() - 1);
		RoomCode.AppendChar(CharacterPool[RandomIndex]);
	}

	return RoomCode;
}

void UMultiplayerSessionsSubsystem::ValidateAndCreateSessionWithCode()
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnCreateSessionComplete.Broadcast(false);
		return;
	}

	bIsCreatingWithRoomCode = true;

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	const TSharedRef<FOnlineSessionSearch> Search = MakeRoomCodeSearch(PendingRoomCode);
	const ULocalPlayer* LocalPlayer = GetSessionLocalPlayer();
	if (!LocalPlayer || !SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), Search))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		bIsCreatingWithRoomCode = false;

		// ★방 만들기 흐름의 실패다★ — 참가용 "방을 찾지 못함" 신호를 보내면 메뉴가 엉뚱한 오류를 띄우고
		// Host 버튼이 비활성으로 남는다.
		CurrentRoomCode.Empty();
		MultiplayerOnCreateSessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::CreateSessionInternal(int32 NumPublicConnections)
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnCreateSessionComplete.Broadcast(false);
		return;
	}

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	LastSessionSettings = MakeShared<FOnlineSessionSettings>();
	LastSessionSettings->bIsLANMatch = IsLanSubsystem();
	LastSessionSettings->NumPublicConnections = NumPublicConnections;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	LastSessionSettings->BuildUniqueId = 1;

	// 룸 코드를 세션 설정에 실어 검색 조건으로 쓸 수 있게 한다
	LastSessionSettings->Set(RoomCodeKey, CurrentRoomCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	const ULocalPlayer* LocalPlayer = GetSessionLocalPlayer();
	if (!LocalPlayer || !SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CurrentRoomCode.Empty();
		MultiplayerOnCreateSessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::TryProcessPendingInvite()
{
	if (!bInvitePending || bInviteJoinStarted || !CachedInviteResult.IsValid()) return;
	if (!SessionInterface.IsValid()) return;
	if (GetSessionLocalPlayer() == nullptr) return;

	bInviteJoinStarted = true;
	bInvitePending = false;

	CachedInviteResult->Session.SessionSettings.bUsesPresence = true;
	CachedInviteResult->Session.SessionSettings.bUseLobbiesIfAvailable = true;

	const FOnlineSessionSearchResult InviteResult = *CachedInviteResult;
	CachedInviteResult.Reset();
	JoinSession(InviteResult);
}
