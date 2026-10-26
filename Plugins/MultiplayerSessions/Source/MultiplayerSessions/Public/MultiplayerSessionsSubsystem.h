// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "MultiplayerSessionsSubsystem.generated.h"

// Declaring our own custom delegates for the Menu class to bind callbacks to
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestroySessionComplete, bool, bWasSuccessful);
// 룸 코드가 정해졌을 때 (방 생성 성공 · 참가 성공)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnRoomCodeGenerated, const FString&, RoomCode);

// Room code configuration
static constexpr int32 ROOM_CODE_LENGTH = 8;

/**
 * 룸 코드 기반 세션 생성 · 검색 · 참가 · 정리를 맡는 서브시스템.
 * 메인 메뉴 맵 경로는 DefaultGame.ini 의 [/Script/MultiplayerSessions.MultiplayerSessionsSubsystem] MainMenuMapPath 로 바꿀 수 있다.
 */
UCLASS(Config = Game)
class MULTIPLAYERSESSIONS_API UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UMultiplayerSessionsSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// To handle session functionality. The Menu class will call these
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void CreateSessionWithRoomCode(int32 NumPublicConnections);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void FindSessionByRoomCode(const FString& RoomCode);
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	void DestroySession();
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void UpdateSessionJoinability(bool bAllowJoin);
	UFUNCTION()
	void LeaveServer();

	UFUNCTION(BlueprintCallable, Category = "VoiceChat")
	void StartVoiceChat();

	UFUNCTION(BlueprintCallable, Category = "Session")
	void StopVoiceChat();

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	const FString& GetCurrentRoomCode() const { return CurrentRoomCode; }

	// Our own custom delegates for the Menu class to bind callbacks to
	FMultiplayerOnCreateSessionComplete MultiplayerOnCreateSessionComplete;
	FMultiplayerOnFindSessionsComplete MultiplayerOnFindSessionsComplete;
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionComplete;
	FMultiplayerOnDestroySessionComplete MultiplayerOnDestroySessionComplete;
	FMultiplayerOnRoomCodeGenerated MultiplayerOnRoomCodeGenerated;

protected:
	// Internal callbacks for the delegates we'll add to the Online Session Interface delegate list.
	// These don't need to be called outside this class.
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

	// 세션을 떠난 뒤 돌아갈 메인 메뉴 맵 (게임 프로젝트가 config 로 지정)
	UPROPERTY(Config)
	FString MainMenuMapPath = TEXT("/Game/Maps/MainMenu");

private:
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	// 맵 로드가 끝나면 보류된 초대를 다시 시도한다
	void HandlePostLoadMap(UWorld* LoadedWorld);

	void TravelToMainMenu(APlayerController* PC) const;

	// 세션 API 에 넘길 로컬 플레이어 (레벨 전환 중이면 nullptr)
	const ULocalPlayer* GetSessionLocalPlayer() const;

	// 룸 코드 검색 요청을 만든다 (참가 검색과 생성 전 중복 검사가 같은 조건을 쓴다)
	TSharedRef<FOnlineSessionSearch> MakeRoomCodeSearch(const FString& RoomCode);

	bool IsLanSubsystem() const;

	IOnlineSessionPtr SessionInterface;
	IOnlineVoicePtr VoiceInterface = nullptr;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	// 룸 코드 상태
	FString CurrentRoomCode;
	FString PendingRoomCode;
	bool bIsCreatingWithRoomCode{false};

	// 룸코드 충돌 재시도 상한 (검색이 계속 결과를 반환하는 이상 동작에서 무한 검색 루프 방지)
	static constexpr int32 MaxRoomCodeRetries = 5;
	int32 RoomCodeRetryCount{0};

	FString GenerateRoomCode();
	void ValidateAndCreateSessionWithCode();
	void CreateSessionInternal(int32 NumPublicConnections);

	// To add to the Online Session Interface delegate list.
	// We'll bind our MultiplayerSessionsSubsystem internal callback to these.
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	FOnUpdateSessionCompleteDelegate UpdateSessionCompleteDelegate;
	FDelegateHandle UpdateSessionCompleteDelegateHandle;
	FOnSessionUserInviteAcceptedDelegate SessionUserInviteAcceptedDelegate;
	FDelegateHandle SessionUserInviteAcceptedDelegateHandle;

	bool bCreateSessionOnDestroy{false};
	int32 LastNumPublicConnections{0};

	// Destroy 완료 후 보류된 Find/Join 재시도를 위한 상태
	bool bFindSessionOnDestroy{false};
	FString PendingFindRoomCode;
	bool bJoinSessionOnDestroy{false};
	TSharedPtr<FOnlineSessionSearchResult> PendingJoinResult;

	// LeaveServer: 세션 파괴 완료 콜백에서 메인메뉴로 이동 (떠나는 월드 기준 콜백 발화 방지)
	bool bTravelToMainMenuOnDestroy{false};

	// 스팀 초대를 받았지만 아직 참가를 시작하지 못한 상태
	bool bInvitePending = false;
	// 초대로 JoinSession 을 이미 시작했다
	bool bInviteJoinStarted = false;

	// 처리 대기 중인 초대 결과
	TSharedPtr<FOnlineSessionSearchResult> CachedInviteResult;

	// 보류된 초대가 있으면 참가를 시도한다
	void TryProcessPendingInvite();
};
