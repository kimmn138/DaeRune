// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Menu.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UMultiplayerSessionsSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMenuSessionRequestSignature);

/**
 *
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void MenuSetup(int32 NumberOfPublicConnections = 4, const FString& LobbyPath = TEXT("/Game/Maps/LobbyMap"));

	// 입력 검증을 통과해 방 만들기 / 참가 요청을 실제로 보낼 때 (로딩 화면을 띄울 시점)
	// ★버튼 클릭에 로딩 화면을 걸지 않는다★ — 룸코드가 비었거나 틀려도 로딩 화면이 뜨고, 닫을 곳이 없다
	UPROPERTY(BlueprintAssignable, Category = "Menu")
	FMenuSessionRequestSignature OnHostRequestStarted;

	UPROPERTY(BlueprintAssignable, Category = "Menu")
	FMenuSessionRequestSignature OnJoinRequestStarted;

	// 방 만들기 / 참가가 실패해 메뉴로 돌아올 때 (띄운 로딩 화면을 닫을 시점 — 방을 못 찾음 포함)
	UPROPERTY(BlueprintAssignable, Category = "Menu")
	FMenuSessionRequestSignature OnSessionRequestFailed;

protected:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Callbacks for the custom delegates on the MultiplayerSessionsSubsystem
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);
	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);

private:
	UPROPERTY(meta = (BindWidget))
	UButton* HostButton;

	UPROPERTY(meta = (BindWidget))
	UButton* JoinButton;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* RoomCodeInputBox;

	// 룸코드 검증/참가 실패 피드백용 (위젯 BP에 없으면 null - 선택 바인딩)
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* ErrorText;

	// 에러 텍스트 표시/숨김 (ErrorText 미바인딩 시 무시)
	void ShowError(const FText& Message);
	void ClearError();

	// 참가 요청이 실패로 끝났다 — 오류 표시 · 참가 버튼 복구 · 실패 알림(로딩 화면 닫기)
	void FailJoin(const FText& Message);

	// 언어 변경 시 디자이너에 박힌 텍스트를 현재 컬처로 다시 밀어넣는다.
	// 플러그인이라 게임 모듈의 SettingsManager(OnLanguageChanged)에 의존할 수 없어 엔진 이벤트를 직접 구독한다.
	void RefreshLocalizedText();

	FDelegateHandle TextRevisionHandle;

	UFUNCTION()
	void HostButtonClicked();

	UFUNCTION()
	void JoinButtonClicked();

	void MenuTearDown();

	// 서브시스템은 GameInstance 수명이라 메뉴보다 오래 산다 — 메뉴가 사라질 때 바인딩을 풀어야 쌓이지 않는다
	void UnbindSubsystemDelegates();

	FDelegateHandle FindSessionsHandle;
	FDelegateHandle JoinSessionHandle;

	// The subsystem designed to handle all online session functionality
	UPROPERTY()
	UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;

	int32 NumPublicConnections{4};
	FString PathToLobby{TEXT("")};

	// 참가를 요청한 룸 코드 (검색 결과에서 같은 코드의 방을 고른다)
	FString PendingJoinRoomCode;
};
