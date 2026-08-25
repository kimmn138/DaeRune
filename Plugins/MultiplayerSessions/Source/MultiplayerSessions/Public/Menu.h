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

protected:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Callbacks for the custom delegates on the MultiplayerSessionsSubsystem
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);
	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);

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

	// 언어 변경 시 디자이너에 박힌 텍스트를 현재 컬처로 다시 밀어넣는다.
	// 플러그인이라 게임 모듈의 SettingsManager(OnLanguageChanged)에 의존할 수 없어 엔진 이벤트를 직접 구독한다.
	void RefreshLocalizedText();

	FDelegateHandle TextRevisionHandle;

	UFUNCTION()
	void HostButtonClicked();

	UFUNCTION()
	void JoinButtonClicked();

	void MenuTearDown();

	// The subsystem designed to handle all online session functionality
	UPROPERTY()
	UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;

	int32 NumPublicConnections{4};
	FString PathToLobby{TEXT("")};

	// �����Ϸ��� �� �ڵ� ����
	FString PendingJoinRoomCode;
};
