// Copyright DaeRune


#include "UI/Widget/DRStageSelectScreenWidget.h"
#include "UI/Widget/DRStageSelectCardWidget.h"
#include "Game/DRGameInstance.h"
#include "Player/DRPlayerController.h"
#include "Components/PanelWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Button.h"
#include "DaeRune/DRLogChannels.h"

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

	// 화면이 FInputModeUIOnly 로 열리므로(ADRPlayerController::OpenScreen)
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
	bStarting = false;

	RefreshAll();
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
	// 다시 그려도 선택은 StageId 로 유지한다. 잠겨 버렸거나 처음 여는 것이면 기본 카드를 고른다
	const int32 PrevIndex = SelectedIndex;
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
		if (IsSelectable(Found))
		{
			SelectedIndex = Found;
		}
	}
	if (SelectedIndex == INDEX_NONE)
	{
		SelectedIndex = PickDefaultSelection();
	}

	RebuildCards();
	UpdateCardStates();
	UpdateButtons();
	OnRefreshStages();

	if (SelectedIndex != PrevIndex)
	{
		OnSelectionChanged(SelectedIndex);
	}
}

int32 UDRStageSelectScreenWidget::PickDefaultSelection() const
{
	if (IsSelectable(InitialSelectedIndex))
	{
		return InitialSelectedIndex;
	}

	// 아직 안 깬 첫 스테이지 — 새로 열린 스테이지가 바로 선택돼 있게 (시안: 2번이 열리면 2번이 선택돼 있다)
	const int32 FirstAvailable = ViewModels.IndexOfByPredicate(
		[](const FDRStageSelectViewModel& ViewModel) { return ViewModel.State == EDRStageSelectState::Available; });
	if (FirstAvailable != INDEX_NONE)
	{
		return FirstAvailable;
	}

	// 다 깼으면 마지막 스테이지
	for (int32 Index = ViewModels.Num() - 1; Index >= 0; --Index)
	{
		if (IsSelectable(Index)) return Index;
	}
	return INDEX_NONE;
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

		// 카드는 그림 크기 그대로, 아래쪽을 맞춘다 (1번 카드는 위 계단만큼 더 높다)
		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			BoxSlot->SetHorizontalAlignment(HAlign_Left);
			BoxSlot->SetVerticalAlignment(VAlign_Bottom);
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
		}
	}
}

void UDRStageSelectScreenWidget::UpdateButtons()
{
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
		OnSelectRejected(Index, ViewModel.LockHint);
		return false;
	}

	if (SelectedIndex == Index) return true;

	SelectedIndex = Index;
	UpdateCardStates();
	UpdateButtons();
	OnSelectionChanged(SelectedIndex);
	return true;
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
		// 이유는 RequestStageTravel 이 로그로 남긴다
		bStarting = false;
		UpdateButtons();
		return false;
	}

	OnStageStarting(ViewModel.StageId);
	return true;
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

void UDRStageSelectScreenWidget::MoveSelection(int32 Delta)
{
	const int32 Num = ViewModels.Num();
	if (bStarting || Num == 0 || Delta == 0) return;

	const int32 Step = Delta > 0 ? 1 : -1;
	int32 Index = HasSelection() ? SelectedIndex : (Step > 0 ? -1 : Num);

	// 잠긴 카드는 건너뛴다. 한 바퀴 돌아도 없으면 그대로
	for (int32 Tries = 0; Tries < Num; ++Tries)
	{
		Index = ((Index + Step) % Num + Num) % Num;
		if (IsSelectable(Index))
		{
			SelectStage(Index);
			return;
		}
	}
}

// ========================= 카드 → 화면 =========================

void UDRStageSelectScreenWidget::NotifyCardHovered(int32 Index)
{
	// 마우스를 올리면 선택이 따라온다. 잠긴 카드는 올리기만 해서는 반응하지 않는다 (누르면 흔들린다)
	if (IsSelectable(Index))
	{
		SelectStage(Index);
	}
}

void UDRStageSelectScreenWidget::NotifyCardClicked(int32 Index)
{
	SelectStage(Index);
}

void UDRStageSelectScreenWidget::NotifyCardDoubleClicked(int32 Index)
{
	// 잠긴 카드는 더블클릭해도 시작하지 않는다 (SelectStage 가 거절)
	if (SelectStage(Index))
	{
		StartSelectedStage();
	}
}

// ========================= 입력 =========================

FReply UDRStageSelectScreenWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Preview(터널링) 단계에서 받는다 — 버튼이 포커스를 가져가도 키가 먼저 여기로 온다
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
	{
		MoveSelection(-1);
		return FReply::Handled();
	}

	if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
	{
		MoveSelection(+1);
		return FReply::Handled();
	}

	if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		StartSelectedStage();
		return FReply::Handled();
	}

	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		RequestClose();
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
	RequestClose();
}

void UDRStageSelectScreenWidget::HandleStatesChanged()
{
	// 디버그 덮어쓰기 등으로 상태가 바뀌었다
	if (bStarting) return;

	RefreshAll();
}
