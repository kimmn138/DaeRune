// Copyright DaeRune


#include "UI/Widget/DRWardrobeSlotWidget.h"
#include "UI/Widget/DRWardrobeScreenWidget.h"

void UDRWardrobeSlotWidget::SetSkinViewModel(const FDRSkinViewModel& InViewModel)
{
	ViewModel = InViewModel;
	OnViewModelUpdated();
}

bool UDRWardrobeSlotWidget::HandleClicked()
{
	UDRWardrobeScreenWidget* Screen = UDRWardrobeScreenWidget::FindOwnerScreen(this);
	if (!Screen) return false;

	// 잠금 판정과 거절 연출은 화면이 담당한다 — 칸은 자기 Id 만 넘긴다
	return Screen->TryEquip(ViewModel.SkinId);
}

FLinearColor UDRWardrobeSlotWidget::GetThumbnailTint() const
{
	return ViewModel.bUnlocked ? FLinearColor::White : LockedTint;
}

FText UDRWardrobeSlotWidget::GetTooltipText() const
{
	return ViewModel.bUnlocked ? ViewModel.Description : ViewModel.UnlockHint;
}
