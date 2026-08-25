// Copyright DaeRune


#include "Menu.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Internationalization/TextLocalizationManager.h"
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

void UMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// 컬처가 바뀌면 엔진이 텍스트 리비전을 올린다. 그 시점에 텍스트를 다시 밀어넣어야
	// 현재 컬처의 번역(번역이 없으면 원문)으로 재해석된다.
	TextRevisionHandle = FTextLocalizationManager::Get().OnTextRevisionChangedEvent.AddUObject(this, &UMenu::RefreshLocalizedText);
}

void UMenu::NativeDestruct()
{
	if (TextRevisionHandle.IsValid())
	{
		FTextLocalizationManager::Get().OnTextRevisionChangedEvent.Remove(TextRevisionHandle);
		TextRevisionHandle.Reset();
	}

	MenuTearDown();

	Super::NativeDestruct();
}

void UMenu::RefreshLocalizedText()
{
	if (!WidgetTree)
	{
		return;
	}

	// 디자이너에 박힌 정적 텍스트는 컬처가 바뀌어도 Slate 캐시가 자동 갱신되지 않는다.
	// SynchronizeProperties를 다시 태워 저장된 FText를 현재 컬처로 재해석시킨다.
	WidgetTree->ForEachWidget([](UWidget* Widget)
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			TextBlock->SynchronizeProperties();
		}
		else if (URichTextBlock* RichTextBlock = Cast<URichTextBlock>(Widget))
		{
			RichTextBlock->SynchronizeProperties();
		}
		else if (UEditableTextBox* EditableTextBox = Cast<UEditableTextBox>(Widget))
		{
			// HintText도 컬처 대상이라 같이 갱신.
			EditableTextBox->SynchronizeProperties();
		}
	});
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

	if (bWasSuccessful && SessionResults.Num() > 0)
	{
		for (const auto& Result : SessionResults)
		{
			FString FoundRoomCode;
			Result.Session.SessionSettings.Get(FName("RoomCode"), FoundRoomCode);

			// 입력한 룸코드와 일치하는지 확인
			if (FoundRoomCode == PendingJoinRoomCode)
			{
				// const 참조이므로 복사본 생성 후 수정
				FOnlineSessionSearchResult ModifiableResult = Result;
				ModifiableResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;
				ModifiableResult.Session.SessionSettings.bUsesPresence = true;
				MultiplayerSessionsSubsystem->JoinSession(ModifiableResult);
				return;
			}
		}
	}

	// 일치하는 방을 찾지 못함 - 사용자 피드백 표시
	ShowError(NSLOCTEXT("MultiplayerSessions", "RoomNotFound", "해당 룸코드의 방을 찾을 수 없습니다."));
	JoinButton->SetIsEnabled(true);
	PendingJoinRoomCode.Empty();
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
		ShowError(NSLOCTEXT("MultiplayerSessions", "JoinFailed", "방 참가에 실패했습니다."));
		JoinButton->SetIsEnabled(true);
		PendingJoinRoomCode.Empty();
	}
}

void UMenu::OnDestroySession(bool bWasSuccessful)
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

	// 룸코드 형식 검증 실패 시 사용자 피드백 표시
	if (InputRoomCode.Len() != ROOM_CODE_LENGTH)
	{
		ShowError(FText::Format(NSLOCTEXT("MultiplayerSessions", "RoomCodeLength", "룸코드는 {0}자리여야 합니다."), FText::AsNumber(ROOM_CODE_LENGTH)));
		return;
	}

	// 영숫자만 허용
	for (TCHAR Char : InputRoomCode)
	{
		if (!FChar::IsAlnum(Char))
		{
			ShowError(NSLOCTEXT("MultiplayerSessions", "RoomCodeAlnum", "룸코드는 영문/숫자만 입력할 수 있습니다."));
			return;
		}
	}

	ClearError();
	JoinButton->SetIsEnabled(false);
	PendingJoinRoomCode = InputRoomCode;

	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->FindSessionByRoomCode(InputRoomCode);
	}
}

void UMenu::ShowError(const FText& Message)
{
	if (ErrorText)
	{
		ErrorText->SetText(Message);
		ErrorText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UMenu::ClearError()
{
	if (ErrorText)
	{
		ErrorText->SetText(FText::GetEmpty());
		ErrorText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMenu::MenuTearDown()
{
	RemoveFromParent();
}

