// Copyright DaeRune


#include "UI/Widget/DRStageSelectScreenWidget.h"
#include "UI/Widget/DRStageSelectCardWidget.h"
#include "Game/DRGameInstance.h"
#include "Player/DRPlayerController.h"
#include "Components/PanelWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "DaeRune/DRLogChannels.h"

#define LOCTEXT_NAMESPACE "DRStageSelect"

UDRGameInstance* UDRStageSelectScreenWidget::GetProgression() const
{
	return GetGameInstance<UDRGameInstance>();
}

bool UDRStageSelectScreenWidget::HasSelection() const
{
	return IsValidIndex(SelectedIndex);
}

int32 UDRStageSelectScreenWidget::GetClearedCount() const
{
	int32 Count = 0;
	for (const FDRStageSelectViewModel& ViewModel : ViewModels)
	{
		if (ViewModel.State == EDRStageSelectState::Cleared) ++Count;
	}
	return Count;
}

// ========================= 수명 =========================

void UDRStageSelectScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 화면이 FInputModeUIOnly 로 열리므로(ADRPlayerController::OpenStageSelectScreen)
	// 키 입력은 위젯이 직접 받아야 한다. 포커스가 없으면 키 이벤트가 아예 오지 않는다.
	SetIsFocusable(true);
	SetKeyboardFocus();

	if (Btn_Start)
	{
		Btn_Start->OnClicked.AddUniqueDynamic(this, &UDRStageSelectScreenWidget::HandleStartClicked);
	}
	if (Btn_Back)
	{
		Btn_Back->OnClicked.AddUniqueDynamic(this, &UDRStageSelectScreenWidget::HandleBackClicked);
	}

	if (UDRGameInstance* GI = GetProgression())
	{
		GI->OnStageSelectStatesChanged.AddUniqueDynamic(this, &UDRStageSelectScreenWidget::HandleStatesChanged);
	}
	else
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] UDRGameInstance 를 찾지 못했습니다. 카드 목록이 비어 있게 됩니다."));
	}

	SelectedIndex = INDEX_NONE;
	FocusedIndex = INDEX_NONE;
	bStarting = false;

	RefreshAll();
	ShowIdleMessage();
}

void UDRStageSelectScreenWidget::NativeDestruct()
{
	if (Btn_Start)
	{
		Btn_Start->OnClicked.RemoveDynamic(this, &UDRStageSelectScreenWidget::HandleStartClicked);
	}
	if (Btn_Back)
	{
		Btn_Back->OnClicked.RemoveDynamic(this, &UDRStageSelectScreenWidget::HandleBackClicked);
	}

	// GameInstance 는 레벨 전환에도 살아남는다. 해제하지 않으면 죽은 위젯이 델리게이트에 남는다.
	if (UDRGameInstance* GI = GetProgression())
	{
		GI->OnStageSelectStatesChanged.RemoveDynamic(this, &UDRStageSelectScreenWidget::HandleStatesChanged);
	}

	Super::NativeDestruct();
}

// ========================= 그리기 =========================

void UDRStageSelectScreenWidget::RefreshAll()
{
	// 다시 그려도 선택은 StageId 로 유지한다. 잠겨 버렸으면 선택을 푼다 (시안 dev 패널 동작)
	const FName PrevSelectedId = HasSelection() ? ViewModels[SelectedIndex].StageId : NAME_None;

	if (const UDRGameInstance* GI = GetProgression())
	{
		GI->GetStageSelectViewModels(ViewModels);
	}
	else
	{
		ViewModels.Reset();
	}

	SelectedIndex = INDEX_NONE;
	if (!PrevSelectedId.IsNone())
	{
		const int32 Found = ViewModels.IndexOfByPredicate(
			[PrevSelectedId](const FDRStageSelectViewModel& ViewModel) { return ViewModel.StageId == PrevSelectedId; });
		if (IsValidIndex(Found) && ViewModels[Found].IsSelectable())
		{
			SelectedIndex = Found;
		}
	}

	if (!IsValidIndex(FocusedIndex))
	{
		FocusedIndex = INDEX_NONE;
	}

	RebuildCards();
	UpdateCardStates();
	UpdateCounters();
	UpdateButtons();
	OnRefreshStages();
}

void UDRStageSelectScreenWidget::RebuildCards()
{
	Cards.Reset();

	if (!Box_Cards || !CardWidgetClass) return;

	Box_Cards->ClearChildren();

	for (int32 Index = 0; Index < ViewModels.Num(); ++Index)
	{
		UDRStageSelectCardWidget* Card = CreateWidget<UDRStageSelectCardWidget>(this, CardWidgetClass);
		if (!Card) continue;

		Card->InitCard(this, ViewModels[Index]);
		Cards.Add(Card);

		UPanelSlot* PanelSlot = Box_Cards->AddChild(Card);

		// 시안: grid-template-columns 1fr 1fr + gap 64px → 같은 폭으로 나누고 사이만 띄운다
		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
			BoxSlot->SetVerticalAlignment(VAlign_Fill);
			BoxSlot->SetPadding(FMargin(Index > 0 ? CardSpacing : 0.0f, 0.0f, 0.0f, 0.0f));
		}
	}
}

void UDRStageSelectScreenWidget::UpdateCardStates()
{
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		if (UDRStageSelectCardWidget* Card = Cards[Index])
		{
			Card->SetSelected(Index == SelectedIndex);
			Card->SetKeyboardFocused(Index == FocusedIndex);
		}
	}
}

void UDRStageSelectScreenWidget::UpdateCounters()
{
	if (Txt_ClearCount)
	{
		Txt_ClearCount->SetText(FText::AsNumber(GetClearedCount()));
	}
	if (Txt_StageCount)
	{
		Txt_StageCount->SetText(FText::AsNumber(GetStageCount()));
	}
}

void UDRStageSelectScreenWidget::UpdateButtons()
{
	// 시안: 선택 전에는 시작 버튼 비활성
	if (Btn_Start)
	{
		Btn_Start->SetIsEnabled(HasSelection() && !bStarting);
	}
	if (Btn_Back)
	{
		Btn_Back->SetIsEnabled(!bStarting);
	}
}

// ========================= 선택 / 시작 =========================

bool UDRStageSelectScreenWidget::SelectStage(int32 Index)
{
	if (bStarting || !IsValidIndex(Index)) return false;

	const FDRStageSelectViewModel& ViewModel = ViewModels[Index];
	if (!ViewModel.IsSelectable())
	{
		if (Cards.IsValidIndex(Index) && Cards[Index])
		{
			Cards[Index]->PlayRejectFeedback();
		}
		ShowLockedMessage(Index);
		OnSelectRejected(Index, ViewModel.LockHint);
		return false;
	}

	const bool bChanged = SelectedIndex != Index;
	SelectedIndex = Index;

	UpdateCardStates();
	UpdateButtons();
	ShowIdleMessage();

	if (bChanged)
	{
		OnSelectionChanged(SelectedIndex);
	}
	return true;
}

void UDRStageSelectScreenWidget::ClearSelection()
{
	if (bStarting) return;

	const bool bChanged = HasSelection();
	SelectedIndex = INDEX_NONE;

	UpdateCardStates();
	UpdateButtons();
	SetMessage(LOCTEXT("MsgCanceled", "선택이 취소되었습니다."));

	if (bChanged)
	{
		OnSelectionChanged(INDEX_NONE);
	}
}

bool UDRStageSelectScreenWidget::StartSelectedStage()
{
	if (bStarting || !HasSelection()) return false;

	const FDRStageSelectViewModel& ViewModel = ViewModels[SelectedIndex];
	if (!ViewModel.IsSelectable()) return false;

	ADRPlayerController* PC = GetOwningPlayer<ADRPlayerController>();
	if (!PC) return false;

	// 이동 요청 전에 잠가 둔다 — ServerTravel 은 다음 틱에 일어나므로 그 사이 연타를 막는다
	bStarting = true;
	UpdateButtons();

	if (!PC->RequestStageTravel(ViewModel.StageId))
	{
		bStarting = false;
		UpdateButtons();
		SetMessage(LOCTEXT("MsgStartFailed", "지금은 출발할 수 없습니다."), true);
		return false;
	}

	SetMessage(FText::Format(LOCTEXT("MsgStarting", "▶ 스테이지 {0} 시작!"), FText::AsNumber(ViewModel.StageNumber)));
	OnStageStarting(ViewModel.StageId);
	return true;
}

void UDRStageSelectScreenWidget::RequestBack()
{
	if (bStarting) return;

	// 시안의 "뒤로"는 선택 취소다. 선택이 없을 때 한 번 더 누르면 화면을 닫는다.
	if (HasSelection())
	{
		ClearSelection();
	}
	else
	{
		RequestClose();
	}
}

void UDRStageSelectScreenWidget::RequestClose()
{
	if (bStarting) return;

	// 닫기는 반드시 컨트롤러를 거친다 — 입력 모드 복구가 거기 묶여 있다.
	if (ADRPlayerController* PC = GetOwningPlayer<ADRPlayerController>())
	{
		PC->CloseStageSelectScreen();
	}
}

void UDRStageSelectScreenWidget::MoveFocus(int32 Delta)
{
	if (bStarting || ViewModels.Num() == 0) return;

	if (!IsValidIndex(FocusedIndex))
	{
		// 첫 입력은 이동하지 않고 시작 카드에 포커스만 준다: 선택된 카드 → InitialFocusIndex → 첫 진행 가능 카드
		if (IsValidIndex(SelectedIndex))
		{
			FocusedIndex = SelectedIndex;
		}
		else if (IsValidIndex(InitialFocusIndex))
		{
			FocusedIndex = InitialFocusIndex;
		}
		else
		{
			FocusedIndex = ViewModels.IndexOfByPredicate(
				[](const FDRStageSelectViewModel& ViewModel) { return ViewModel.IsSelectable(); });
			if (!IsValidIndex(FocusedIndex)) FocusedIndex = 0;
		}
	}
	else
	{
		const int32 Num = ViewModels.Num();
		FocusedIndex = ((FocusedIndex + Delta) % Num + Num) % Num;
	}

	UpdateCardStates();
	ShowCardMessage(FocusedIndex);
}

// ========================= 카드 → 화면 =========================

void UDRStageSelectScreenWidget::NotifyCardHovered(int32 Index)
{
	if (bStarting || !IsValidIndex(Index)) return;

	// 마우스가 잡으면 키보드 포커스 표시는 내려놓는다 (두 카드가 동시에 강조되지 않게)
	if (FocusedIndex != INDEX_NONE)
	{
		FocusedIndex = INDEX_NONE;
		UpdateCardStates();
	}

	ShowCardMessage(Index);
}

void UDRStageSelectScreenWidget::NotifyCardUnhovered(int32 Index)
{
	if (bStarting) return;

	ShowIdleMessage();
}

void UDRStageSelectScreenWidget::NotifyCardClicked(int32 Index)
{
	SelectStage(Index);
}

void UDRStageSelectScreenWidget::NotifyCardDoubleClicked(int32 Index)
{
	// 시안: 잠긴 카드는 더블클릭해도 시작하지 않는다 (SelectStage 가 거절)
	if (SelectStage(Index))
	{
		StartSelectedStage();
	}
}

// ========================= 입력 =========================

FReply UDRStageSelectScreenWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Preview(터널링) 단계에서 받는다 — 시작/뒤로 버튼이 포커스를 가져가도 키가 먼저 여기로 온다
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
	{
		MoveFocus(-1);
		return FReply::Handled();
	}

	if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
	{
		MoveFocus(+1);
		return FReply::Handled();
	}

	if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		// 포커스된 카드가 아직 선택 전이면 선택, 이미 선택된 상태면 시작
		if (IsValidIndex(FocusedIndex) && FocusedIndex != SelectedIndex)
		{
			SelectStage(FocusedIndex);
		}
		else
		{
			StartSelectedStage();
		}
		return FReply::Handled();
	}

	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		RequestBack();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UDRStageSelectScreenWidget::HandleStartClicked()
{
	StartSelectedStage();
}

void UDRStageSelectScreenWidget::HandleBackClicked()
{
	RequestBack();
}

void UDRStageSelectScreenWidget::HandleStatesChanged()
{
	// 디버그 덮어쓰기 등으로 상태가 바뀌었다
	if (bStarting) return;

	RefreshAll();
	ShowIdleMessage();
}

// ========================= 문구 =========================

void UDRStageSelectScreenWidget::SetMessage(const FText& Message, bool bIsWarning)
{
	if (Txt_Message)
	{
		Txt_Message->SetText(Message);
		Txt_Message->SetColorAndOpacity(FSlateColor(bIsWarning ? WarningMessageColor : MessageColor));
	}
	OnMessageChanged(Message, bIsWarning);
}

void UDRStageSelectScreenWidget::ShowIdleMessage()
{
	if (HasSelection())
	{
		SetMessage(FText::Format(LOCTEXT("MsgSelected", "스테이지 {0} 선택됨. 시작 버튼 또는 더블클릭"),
			FText::AsNumber(ViewModels[SelectedIndex].StageNumber)));
	}
	else
	{
		SetMessage(LOCTEXT("MsgIdle", "스테이지를 선택하세요."));
	}
}

void UDRStageSelectScreenWidget::ShowCardMessage(int32 Index)
{
	if (!IsValidIndex(Index)) return;

	const FDRStageSelectViewModel& ViewModel = ViewModels[Index];
	switch (ViewModel.State)
	{
	case EDRStageSelectState::Locked:
		ShowLockedMessage(Index);
		break;

	case EDRStageSelectState::Cleared:
		SetMessage(FText::Format(LOCTEXT("MsgHoverCleared", "스테이지 {0} — 클리어 완료 · 다시 플레이 가능"),
			FText::AsNumber(ViewModel.StageNumber)));
		break;

	case EDRStageSelectState::Available:
	default:
		SetMessage(FText::Format(LOCTEXT("MsgHoverAvailable", "스테이지 {0} — 클릭하여 선택"),
			FText::AsNumber(ViewModel.StageNumber)));
		break;
	}
}

void UDRStageSelectScreenWidget::ShowLockedMessage(int32 Index)
{
	if (!IsValidIndex(Index)) return;

	SetMessage(FText::Format(LOCTEXT("MsgLocked", "잠긴 스테이지입니다. {0}"), ViewModels[Index].LockHint), true);
}

#undef LOCTEXT_NAMESPACE
