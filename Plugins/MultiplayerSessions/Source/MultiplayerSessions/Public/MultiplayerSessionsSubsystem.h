// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MultiplayerSessionsSubsystem.generated.h"

// Declaring our own custom delegates for the Menu class to bind callbacks to
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestroySessionComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartSessionComplete, bool, bWasSuccessful);
// 방 코드 생성 완료 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnRoomCodeGenerated, const FString&, RoomCode);

/**
 * 
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	UMultiplayerSessionsSubsystem();

	// To handle session functionality. The Menu class will call these
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void CreateSessionWithRoomCode(int32 NumPublicConnections, const FString& MatchType = "RoomCodeOnly");

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void FindSessionByRoomCode(const FString& RoomCode);
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	void DestroySession();
	void StartSession();
	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	void UpdateSessionJoinability(bool bAllowJoin);

	UFUNCTION(BlueprintCallable, Category = "Multiplayer Sessions")
	FString GetCurrentRoomCode() const { return CurrentRoomCode; }

	// Our own custom delegates for the Menu class to bind callbacks to
	FMultiplayerOnCreateSessionComplete MultiplayerOnCreateSessionComplete;
	FMultiplayerOnFindSessionsComplete MultiplayerOnFindSessionsComplete;
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionComplete;
	FMultiplayerOnDestroySessionComplete MultiplayerOnDestroySessionComplete;
	FMultiplayerOnStartSessionComplete MultiplayerOnStartSessionComplete;
	FMultiplayerOnRoomCodeGenerated MultiplayerOnRoomCodeGenerated;

protected:
	// Internal callbacks for the delegates we'll add to the Online Session Interface delegate list.
	// These don't need to be called outside this class.
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnSessionUserInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);

private:
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	// 방 코드 관련 변수들
	FString CurrentRoomCode;
	FString PendingRoomCode;
	FString SearchingRoomCode;
	bool bIsCreatingWithRoomCode{false};

	// 방 코드 생성 함수
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
	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	FDelegateHandle StartSessionCompleteDelegateHandle;
	FOnUpdateSessionCompleteDelegate UpdateSessionCompleteDelegate;
	FDelegateHandle UpdateSessionCompleteDelegateHandle;
	FOnSessionUserInviteAcceptedDelegate SessionUserInviteAcceptedDelegate;
	FDelegateHandle SessionUserInviteAcceptedDelegateHandle;

	bool bCreateSessionOnDestroy{false};
	int32 LastNumPublicConnections;

	// 초대가 저장되어 있는 상태
	bool bInvitePending = false;
	// JoinSession을 이미 시작
	bool bInviteJoinStarted = false;

	// 초대 정보 캐싱
	TSharedPtr<FOnlineSessionSearchResult> CachedInviteResult;

	// 안전한 Join 시도 함수
	void TryProcessPendingInvite();
};
