// Copyright DaeRune


#include "UI/Widget/DRWardrobeTabWidget.h"
#include "UI/Widget/DRWardrobeScreenWidget.h"

void UDRWardrobeTabWidget::SetSelected(bool bInSelected)
{
	// 상태가 그대로면 BP 이벤트를 발행하지 않는다 (매 갱신마다 연출이 다시 트리거되는 것 방지)
	if (bSelected == bInSelected) return;

	bSelected = bInSelected;
	OnSelectionChanged(bSelected);
}

void UDRWardrobeTabWidget::HandleClicked()
{
	if (UDRWardrobeScreenWidget* Screen = UDRWardrobeScreenWidget::FindOwnerScreen(this))
	{
		Screen->SetCurrentCategory(Category);
	}
}

void UDRWardrobeTabWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 화면이 먼저 만들어지고 탭이 나중에 붙는 순서에서도 선택 표시가 맞게 한다.
	// (UDRWardrobeScreenWidget::UpdateTabSelection 은 NativeConstruct 시점에 한 번 돌지만,
	//  그때 아직 이 탭이 트리에 없을 수 있다)
	if (const UDRWardrobeScreenWidget* Screen = UDRWardrobeScreenWidget::FindOwnerScreen(this))
	{
		const bool bShouldSelect = (Screen->GetCurrentCategory() == Category);

		// 초기 1회는 상태가 같아도 BP 이벤트를 발행해 시각 상태를 확실히 맞춘다
		bSelected = bShouldSelect;
		OnSelectionChanged(bSelected);
	}
}
