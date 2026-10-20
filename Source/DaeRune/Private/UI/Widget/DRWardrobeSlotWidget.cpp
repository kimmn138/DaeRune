// Copyright DaeRune


#include "UI/Widget/DRWardrobeSlotWidget.h"
#include "UI/Widget/DRWardrobeScreenWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UDRWardrobeSlotWidget::SetSkinViewModel(const FDRSkinViewModel& InViewModel)
{
	ViewModel = InViewModel;

	// BindWidget 포인터는 CreateWidget(Initialize) 시점에 이미 채워져 있다 — 화면에 붙기 전에 그려도 된다
	ApplyViewModelVisuals();
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

void UDRWardrobeSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Slot)
	{
		Button_Slot->OnClicked.AddUniqueDynamic(this, &UDRWardrobeSlotWidget::HandleSlotButtonClicked);
		Button_Slot->OnHovered.AddUniqueDynamic(this, &UDRWardrobeSlotWidget::HandleSlotButtonHovered);
		Button_Slot->OnUnhovered.AddUniqueDynamic(this, &UDRWardrobeSlotWidget::HandleSlotButtonUnhovered);
	}

	ApplyViewModelVisuals();
}

void UDRWardrobeSlotWidget::NativeDestruct()
{
	if (Button_Slot)
	{
		Button_Slot->OnClicked.RemoveDynamic(this, &UDRWardrobeSlotWidget::HandleSlotButtonClicked);
		Button_Slot->OnHovered.RemoveDynamic(this, &UDRWardrobeSlotWidget::HandleSlotButtonHovered);
		Button_Slot->OnUnhovered.RemoveDynamic(this, &UDRWardrobeSlotWidget::HandleSlotButtonUnhovered);
	}

	bHovering = false;

	Super::NativeDestruct();
}

void UDRWardrobeSlotWidget::HandleSlotButtonClicked()
{
	// 성공하면 화면이 목록을 통째로 다시 그리므로 여기서 선택 표시를 바꾸지 않는다
	HandleClicked();
}

void UDRWardrobeSlotWidget::HandleSlotButtonHovered()
{
	bHovering = true;
	UpdateStateImage();
}

void UDRWardrobeSlotWidget::HandleSlotButtonUnhovered()
{
	bHovering = false;
	UpdateStateImage();
}

void UDRWardrobeSlotWidget::UpdateStateImage()
{
	if (!Image_State) return;

	// 잠긴 칸도 기본 이미지를 쓴다 — 전용 잠금 아트가 아직 없다 (D-18)
	UTexture2D* Texture = ViewModel.bEquipped ? StateSelectedTexture.Get()
		: (bHovering ? StateHoveredTexture.Get() : StateNormalTexture.Get());

	if (Texture)
	{
		Image_State->SetBrushFromTexture(Texture, false);
	}
}

void UDRWardrobeSlotWidget::ApplyViewModelVisuals()
{
	UpdateStateImage();

	const bool bHasThumbnail = HasThumbnail();

	if (Image_Thumbnail)
	{
		if (bHasThumbnail)
		{
			Image_Thumbnail->SetBrushFromTexture(ViewModel.PreviewIcon, false);
			Image_Thumbnail->SetColorAndOpacity(GetThumbnailTint());
			Image_Thumbnail->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			// 빈 브러시는 흰 사각형으로 그려진다 (Plan(옷장) T-16)
			Image_Thumbnail->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (Txt_Name)
	{
		// 아이콘이 생기기 전까지는 이름 글자가 칸을 구분해 준다
		Txt_Name->SetText(ViewModel.DisplayName);
		Txt_Name->SetVisibility(bHasThumbnail ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		Txt_Name->SetRenderOpacity(ViewModel.bUnlocked ? 1.0f : LockedNameOpacity);
	}

	if (Image_Lock)
	{
		// 자물쇠 아트가 지정됐을 때만 켠다 — 브러시가 비어 있으면 흰 사각형이 된다
		const bool bHasLockArt = Image_Lock->GetBrush().GetResourceObject() != nullptr;
		Image_Lock->SetVisibility((!ViewModel.bUnlocked && bHasLockArt)
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Button_Slot)
	{
		// ★잠긴 칸도 활성으로 둔다★ — 비활성이면 툴팁이 뜨지 않는다 (D-14)
		Button_Slot->SetToolTipText(GetTooltipText());
	}
}
