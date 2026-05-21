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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartSessionComplete, bool, bWasSuccessful);
// �� �ڵ� ���� �Ϸ� ��������Ʈ
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnRoomCodeGenerated, const FString&, RoomCode);

// Room code configuration
static constexpr int32 ROOM_CODE_LENGTH = 8;

/**
 *
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	UMultiplayerSessionsSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

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
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	IOnlineSessionPtr SessionInterface;
	IOnlineVoicePtr VoiceInterface = nullptr;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	// �� �ڵ� ���� ������
	FString CurrentRoomCode;
	FString PendingRoomCode;
	FString SearchingRoomCode;
	bool bIsCreatingWithRoomCode{false};

	// �� �ڵ� ���� �Լ�
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

	// Destroy 완료 후 보류된 Find/Join 재시도를 위한 상태
	bool bFindSessionOnDestroy{false};
	FString PendingFindRoomCode;
	bool bJoinSessionOnDestroy{false};
	TSharedPtr<FOnlineSessionSearchResult> PendingJoinResult;

	// �ʴ밡 ����Ǿ� �ִ� ����
	bool bInvitePending = false;
	// JoinSession�� �̹� ����
	bool bInviteJoinStarted = false;

	// �ʴ� ���� ĳ��
	TSharedPtr<FOnlineSessionSearchResult> CachedInviteResult;

	// ������ Join �õ� �Լ�
	void TryProcessPendingInvite();
};
