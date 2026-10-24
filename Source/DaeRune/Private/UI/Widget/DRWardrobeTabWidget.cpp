// Copyright DaeRune


#include "UI/Widget/DRWardrobeTabWidget.h"
#include "UI/Widget/DRWardrobeScreenWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UDRWardrobeTabWidget::SetSelected(bool bInSelected)
{
	// 상태가 그대로면 BP 이벤트를 발행하지 않는다 (매 갱신마다 연출이 다시 트리거되는 것 방지)
	if (bSelected == bInSelected) return;

	bSelected = bInSelected;
	ApplySelectionVisuals();
	OnSelectionChanged(bSelected);
}

void UDRWardrobeTabWidget::HandleClicked()
{
	if (UDRWardrobeScreenWidget* Screen = UDRWardrobeScreenWidget::FindOwnerScreen(this))
	{
		Screen->SetCurrentCategory(Category);
	}
}

void UDRWardrobeTabWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너 미리보기에서도 라벨과 비선택 모양이 보이게 한다
	if (Text_Label && !LabelText.IsEmpty())
	{
		Text_Label->SetText(LabelText);
	}
	ApplySelectionVisuals();
}

void UDRWardrobeTabWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Tab)
	{
		Button_Tab->OnClicked.AddUniqueDynamic(this, &UDRWardrobeTabWidget::HandleClicked);
	}

	// 화면이 먼저 만들어지고 탭이 나중에 붙는 순서에서도 선택 표시가 맞게 한다.
	// (UDRWardrobeScreenWidget::UpdateTabSelection 은 NativeConstruct 시점에 한 번 돌지만,
	//  그때 아직 이 탭이 트리에 없을 수 있다)
	if (const UDRWardrobeScreenWidget* Screen = UDRWardrobeScreenWidget::FindOwnerScreen(this))
	{
		// 초기 1회는 상태가 같아도 그림과 BP 이벤트를 확실히 맞춘다
		bSelected = (Screen->GetCurrentCategory() == Category);
		ApplySelectionVisuals();
		OnSelectionChanged(bSelected);
	}
}

void UDRWardrobeTabWidget::NativeDestruct()
{
	if (Button_Tab)
	{
		Button_Tab->OnClicked.RemoveDynamic(this, &UDRWardrobeTabWidget::HandleClicked);
	}

	Super::NativeDestruct();
}

void UDRWardrobeTabWidget::ApplySelectionVisuals()
{
	if (Image_Pill)
	{
		// 필은 버튼 아래에 깔리는 그림일 뿐이다 — 클릭을 먹지 않게 한다
		Image_Pill->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Text_Label)
	{
		// 라벨은 오버레이에서 버튼 위에 있다 — 클릭을 먹으면 글자 위를 누를 때 버튼이 안 눌린다 (Plan(옷장) T-41)
		Text_Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		Text_Label->SetColorAndOpacity(bSelected ? SelectedLabelColor : UnselectedLabelColor);
	}
}
