// Copyright DaeRune


#include "UI/Widget/DRStageSelectCardWidget.h"
#include "UI/Widget/DRStageSelectScreenWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "DRStageSelect"

void UDRStageSelectCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// UUserWidget 기본값(SelfHitTestInvisible)이면 카드 자신이 호버/클릭의 주인이 되지 못한다
	SetVisibility(ESlateVisibility::Visible);
}

void UDRStageSelectCardWidget::InitCard(UDRStageSelectScreenWidget* InOwnerScreen, const FDRStageSelectViewModel& InViewModel)
{
	OwnerScreen = InOwnerScreen;
	ViewModel = InViewModel;

	ApplyViewModelVisuals();
	UpdateFrameVisuals();
	OnViewModelUpdated();
}

void UDRStageSelectCardWidget::SetSelected(bool bInSelected)
{
	if (bSelected == bInSelected) return;

	bSelected = bInSelected;

	if (Panel_Selected)
	{
		Panel_Selected->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	UpdateFrameVisuals();
	OnSelectedChanged(bSelected);
}

void UDRStageSelectCardWidget::SetKeyboardFocused(bool bInFocused)
{
	if (bKeyboardFocused == bInFocused) return;

	const bool bWasHighlighted = IsHighlighted();
	bKeyboardFocused = bInFocused;

	UpdateFrameVisuals();
	if (bWasHighlighted != IsHighlighted())
	{
		OnHighlightChanged(IsHighlighted());
	}
}

void UDRStageSelectCardWidget::SetHovered(bool bInHovered)
{
	if (bHovered == bInHovered) return;

	const bool bWasHighlighted = IsHighlighted();
	bHovered = bInHovered;

	UpdateFrameVisuals();
	if (bWasHighlighted != IsHighlighted())
	{
		OnHighlightChanged(IsHighlighted());
	}
}

void UDRStageSelectCardWidget::PlayRejectFeedback()
{
	// 시안: 클래스를 뗐다 다시 붙여 애니메이션을 처음부터 — 여기서는 남은 시간을 다시 채운다
	ShakeTimeRemaining = ShakeDuration;
	OnSelectRejected();
}

FText UDRStageSelectCardWidget::GetStateLabel(EDRStageSelectState State)
{
	switch (State)
	{
	case EDRStageSelectState::Available: return LOCTEXT("BadgeAvailable", "진행 가능");
	case EDRStageSelectState::Cleared:   return LOCTEXT("BadgeCleared", "CLEAR");
	case EDRStageSelectState::Locked:
	default:                             return LOCTEXT("BadgeLocked", "LOCKED");
	}
}

void UDRStageSelectCardWidget::ApplyViewModelVisuals()
{
	const bool bLocked = ViewModel.State == EDRStageSelectState::Locked;
	const bool bCleared = ViewModel.State == EDRStageSelectState::Cleared;

	if (Txt_Number)
	{
		// "01", "02" ...
		FNumberFormattingOptions Options;
		Options.SetMinimumIntegralDigits(2);
		Options.SetUseGrouping(false);
		Txt_Number->SetText(FText::AsNumber(ViewModel.StageNumber, &Options));
	}

	if (Txt_Badge)
	{
		Txt_Badge->SetText(GetStateLabel(ViewModel.State));
	}

	if (Txt_Title)
	{
		Txt_Title->SetText(FText::Format(LOCTEXT("CardTitleFmt", "스테이지 {0} · {1}"),
			FText::AsNumber(ViewModel.StageNumber), ViewModel.DisplayName));
	}

	if (Txt_Mission)
	{
		Txt_Mission->SetText(ViewModel.MissionText);
	}

	if (Txt_Done)
	{
		Txt_Done->SetText(LOCTEXT("CardDone", "임무 완료 · 다시 도전 가능"));
		Txt_Done->SetVisibility(bCleared ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 덮개류는 클릭을 가로채지 않도록 HitTestInvisible 로 켠다
	if (Panel_Cleared)
	{
		Panel_Cleared->SetVisibility(bCleared ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Panel_Locked)
	{
		Panel_Locked->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Panel_Selected)
	{
		Panel_Selected->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Image_Thumbnail)
	{
		// 아트가 아직 없으므로 이미지가 지정된 스테이지만 브러시를 바꾼다 (없으면 WBP 기본 브러시 유지)
		if (ViewModel.Thumbnail)
		{
			Image_Thumbnail->SetBrushFromTexture(ViewModel.Thumbnail, /*bMatchSize*/ false);
		}
		Image_Thumbnail->SetColorAndOpacity(bLocked ? LockedThumbnailTint : FLinearColor::White);
	}
}

void UDRStageSelectCardWidget::UpdateFrameVisuals()
{
	const bool bLocked = ViewModel.State == EDRStageSelectState::Locked;

	FLinearColor Frame;
	if (bLocked)
	{
		// 시안 .locked:hover — 떠오르지 않고 경고색으로만 반응한다
		Frame = IsHighlighted() ? WarningColor : LockedColor;
	}
	else
	{
		Frame = (IsHighlighted() || bSelected) ? HighlightColor : FrameColor;
	}

	if (Border_Frame)
	{
		// 둥근 사각형 외곽선은 BrushColor 로 물들지 않는다 — 외곽선 색 자체를 바꾼다.
		// (이미지 · 9-slice 브러시로 바꾸면 그때는 BrushColor 로 틴트한다)
		if (Border_Frame->Background.DrawAs == ESlateBrushDrawType::RoundedBox && Border_Frame->Background.OutlineSettings.Width > 0.0f)
		{
			FSlateBrush Brush = Border_Frame->Background;
			Brush.OutlineSettings.Color = FSlateColor(Frame);
			Border_Frame->SetBrush(Brush);
		}
		else
		{
			Border_Frame->SetBrushColor(Frame);
		}
	}

	const FLinearColor BadgeColor = bLocked ? LockedColor : HighlightColor;
	if (Txt_Badge)
	{
		Txt_Badge->SetColorAndOpacity(FSlateColor(BadgeColor));
	}
	if (Border_Badge)
	{
		// 배경색(BrushColor)은 그대로 두고 외곽선만 바꾼다
		FSlateBrush Brush = Border_Badge->Background;
		Brush.OutlineSettings.Color = FSlateColor(BadgeColor);
		Border_Badge->SetBrush(Brush);
	}
}

void UDRStageSelectCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bUseDefaultMotion) return;

	const bool bLocked = ViewModel.State == EDRStageSelectState::Locked;
	const float TargetAlpha = (!bLocked && (IsHighlighted() || bSelected)) ? 1.0f : 0.0f;
	const bool bShaking = ShakeTimeRemaining > 0.0f;

	// 정지 상태면 매 프레임 트랜스폼을 다시 쓰지 않는다
	if (!bShaking && FMath::IsNearlyEqual(HighlightAlpha, TargetAlpha, KINDA_SMALL_NUMBER)) return;

	HighlightAlpha = FMath::FInterpTo(HighlightAlpha, TargetAlpha, InDeltaTime, HighlightInterpSpeed);
	if (FMath::IsNearlyEqual(HighlightAlpha, TargetAlpha, 0.001f))
	{
		HighlightAlpha = TargetAlpha;
	}

	// 시안 @keyframes shake: 25% → -8px, 75% → +8px
	float ShakeOffset = 0.0f;
	if (bShaking)
	{
		ShakeTimeRemaining = FMath::Max(0.0f, ShakeTimeRemaining - InDeltaTime);
		const float Progress = ShakeDuration > 0.0f ? 1.0f - ShakeTimeRemaining / ShakeDuration : 1.0f;
		ShakeOffset = -ShakeAmplitude * FMath::Sin(Progress * 2.0f * PI);
	}

	FWidgetTransform Transform;
	Transform.Translation = FVector2D(ShakeOffset, -HighlightLift * HighlightAlpha);
	Transform.Scale = FVector2D(FMath::Lerp(1.0f, HighlightScale, HighlightAlpha));
	SetRenderTransform(Transform);
}

void UDRStageSelectCardWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	SetHovered(true);
	if (UDRStageSelectScreenWidget* Screen = OwnerScreen.Get())
	{
		Screen->NotifyCardHovered(ViewModel.Index);
	}
}

void UDRStageSelectCardWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	SetHovered(false);
	if (UDRStageSelectScreenWidget* Screen = OwnerScreen.Get())
	{
		Screen->NotifyCardUnhovered(ViewModel.Index);
	}
}

FReply UDRStageSelectCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (UDRStageSelectScreenWidget* Screen = OwnerScreen.Get())
		{
			Screen->NotifyCardClicked(ViewModel.Index);
		}
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UDRStageSelectCardWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Slate 는 두 번째 클릭을 ButtonDown 대신 DoubleClick 으로 보낸다 — 첫 클릭에서 선택은 이미 됐다
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (UDRStageSelectScreenWidget* Screen = OwnerScreen.Get())
		{
			Screen->NotifyCardDoubleClicked(ViewModel.Index);
		}
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

#undef LOCTEXT_NAMESPACE
