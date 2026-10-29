// Copyright DaeRune


#include "UI/Widget/DRStageSelectCardWidget.h"
#include "UI/Widget/DRStageSelectScreenWidget.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

#define LOCTEXT_NAMESPACE "DRStageSelect"

namespace DRStageCard
{
	// 잠금 효과 머티리얼(M_UI_StageSelect_LockBlend) 파라미터
	const FName TextureParam(TEXT("Texture"));
	const FName LockAmountParam(TEXT("LockAmount"));

	// 카드 그림은 데이터 에셋의 소프트 참조라 화면을 처음 열 때 불러온다
	UTexture2D* Load(const TSoftObjectPtr<UTexture2D>& Soft)
	{
		return Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
	}

	FVector2D TextureSize(UTexture2D* Texture)
	{
#if WITH_EDITOR
		// 에디터는 텍스처를 비동기로 컴파일한다 — 끝나기 전엔 크기가 자리표시 그림 크기로 나와서
		// PIE 에서 처음 불러온 카드 그림이 몇 px 로 그려졌다. 쿡된 게임은 해당 없다
		if (Texture->IsCompiling())
		{
			FTextureCompilingManager::Get().FinishCompilation({ Texture });
		}
#endif
		return FVector2D(Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight());
	}

	FSlateBrush MakeBrush(UObject* Resource, const FVector2D& Size)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetResourceObject(Resource);
		Brush.ImageSize = Size;
		return Brush;
	}
}

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

	ApplyStageVisuals();
	ApplyStateVisuals();
	OnViewModelUpdated();
}

void UDRStageSelectCardWidget::SetSelected(bool bInSelected)
{
	if (bSelected == bInSelected) return;

	bSelected = bInSelected;
	ApplyStateVisuals();
	OnSelectedChanged(bSelected);
}

void UDRStageSelectCardWidget::PlayRejectFeedback()
{
	// 연타해도 처음부터 다시 흔들린다
	ShakeTimeRemaining = ShakeDuration;
	OnSelectRejected();
}

void UDRStageSelectCardWidget::ApplyStageVisuals()
{
	using namespace DRStageCard;
	const FDRStageCardArt& Art = ViewModel.CardArt;

	// 카드마다 모양 높이가 다르다(1번 670 = 위 계단 포함, 2번 661). 화면은 아래쪽을 맞춘다.
	if (SizeBox_Card && Art.CardSize.X > 0.0 && Art.CardSize.Y > 0.0)
	{
		SizeBox_Card->SetWidthOverride(Art.CardSize.X);
		SizeBox_Card->SetHeightOverride(Art.CardSize.Y);
	}

	// 그림 · 배지는 잠금 효과를 받으므로 머티리얼로 그린다 (켜고 끄는 것은 ApplyStateVisuals 의 LockAmount)
	SetLockBlendImage(Image_Thumbnail, ThumbnailMID, ViewModel.Thumbnail);
	SetLockBlendImage(Image_Badge, BadgeMID, Load(Art.BadgeImage));

	UTexture2D* Number = Load(Art.NumberImage);
	if (Number)
	{
		const FVector2D Size = TextureSize(Number);
		SetImageTexture(Image_Number, Number, FVector2D(Size.X * NumberHeight / FMath::Max(Size.Y, 1.0), NumberHeight));
	}
	else
	{
		SetImageTexture(Image_Number, nullptr);
	}

	if (Panel_Content)
	{
		Panel_Content->SetRenderTranslation(Art.ContentOffset);
	}

	if (Txt_Title)
	{
		Txt_Title->SetText(ViewModel.DisplayName);
	}
	if (Txt_Badge)
	{
		Txt_Badge->SetText(FText::Format(LOCTEXT("BadgeFmt", "스테이지 {0}"), FText::AsNumber(ViewModel.StageNumber)));
	}
	if (Txt_Mission)
	{
		Txt_Mission->SetText(ViewModel.MissionText);
	}
}

void UDRStageSelectCardWidget::ApplyStateVisuals()
{
	using namespace DRStageCard;
	const FDRStageCardArt& Art = ViewModel.CardArt;
	const bool bLocked = ViewModel.State == EDRStageSelectState::Locked;
	const bool bCleared = ViewModel.State == EDRStageSelectState::Cleared;

	// 베이스 · 테두리 교체. 잠금 그림이 없으면 기본 그림으로 폴백한다 (데이터 검증이 경고한다)
	UTexture2D* BaseTexture = (bLocked && !Art.LockedBase.IsNull()) ? Load(Art.LockedBase) : Load(Art.Base);

	UTexture2D* FrameTexture = nullptr;
	if (bLocked && !Art.LockedOutline.IsNull())
	{
		FrameTexture = Load(Art.LockedOutline);
	}
	else if (bSelected && !Art.SelectBorder.IsNull())
	{
		FrameTexture = Load(Art.SelectBorder);
	}
	else
	{
		FrameTexture = Load(Art.Outline);
	}

	SetImageTexture(Image_Base, BaseTexture);
	SetImageTexture(Image_Frame, FrameTexture);

	// 잠금 효과 — 베이스/테두리는 텍스처에 구워져 있고, 그림 · 배지는 머티리얼, 글자는 색으로 맞춘다
	const float LockAmount = bLocked ? 1.0f : 0.0f;
	if (ThumbnailMID)
	{
		ThumbnailMID->SetScalarParameterValue(LockAmountParam, LockAmount);
	}
	if (BadgeMID)
	{
		BadgeMID->SetScalarParameterValue(LockAmountParam, LockAmount);
	}

	if (Image_Number)
	{
		// 잠기면 묶음(Panel_Content) 전체가 40% 가 되므로 번호 자체는 100% 로 둔다
		Image_Number->SetRenderOpacity((bLocked || bSelected) ? 1.0f : NumberIdleOpacity);
	}
	if (Panel_Content)
	{
		Panel_Content->SetRenderOpacity(bLocked ? LockedContentOpacity : 1.0f);
	}

	const FSlateColor Color(bLocked ? LockedTextColor : TextColor);
	for (UTextBlock* Text : { Txt_Title.Get(), Txt_Badge.Get(), Txt_Mission.Get() })
	{
		if (Text)
		{
			Text->SetColorAndOpacity(Color);
		}
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
}

void UDRStageSelectCardWidget::SetImageTexture(UImage* Image, UTexture2D* Texture, FVector2D DrawSize)
{
	if (!Image) return;

	if (!Texture)
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Image->SetBrush(DRStageCard::MakeBrush(Texture, DrawSize.IsNearlyZero() ? DRStageCard::TextureSize(Texture) : DrawSize));
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UDRStageSelectCardWidget::SetLockBlendImage(UImage* Image, TObjectPtr<UMaterialInstanceDynamic>& Mid, UTexture2D* Texture)
{
	if (!Image) return;

	if (!Texture || !LockBlendMaterial)
	{
		SetImageTexture(Image, Texture);
		return;
	}

	if (!Mid)
	{
		Mid = UMaterialInstanceDynamic::Create(LockBlendMaterial, this);
	}
	Mid->SetTextureParameterValue(DRStageCard::TextureParam, Texture);

	Image->SetBrush(DRStageCard::MakeBrush(Mid, DRStageCard::TextureSize(Texture)));
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UDRStageSelectCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bUseDefaultMotion) return;

	const bool bLocked = ViewModel.State == EDRStageSelectState::Locked;
	const float TargetAlpha = (!bLocked && bSelected) ? 1.0f : 0.0f;
	const bool bShaking = ShakeTimeRemaining > 0.0f;

	// 정지 상태면 매 프레임 트랜스폼을 다시 쓰지 않는다
	if (!bShaking && FMath::IsNearlyEqual(LiftAlpha, TargetAlpha, KINDA_SMALL_NUMBER)) return;

	LiftAlpha = FMath::FInterpTo(LiftAlpha, TargetAlpha, InDeltaTime, LiftInterpSpeed);
	if (FMath::IsNearlyEqual(LiftAlpha, TargetAlpha, 0.001f))
	{
		LiftAlpha = TargetAlpha;
	}

	// 25% → -8px, 75% → +8px
	float ShakeOffset = 0.0f;
	if (bShaking)
	{
		ShakeTimeRemaining = FMath::Max(0.0f, ShakeTimeRemaining - InDeltaTime);
		const float Progress = ShakeDuration > 0.0f ? 1.0f - ShakeTimeRemaining / ShakeDuration : 1.0f;
		ShakeOffset = -ShakeAmplitude * FMath::Sin(Progress * 2.0f * PI);
	}

	// 피벗은 카드 가운데 (Figma 선택 카드도 가운데 기준 1.015배)
	FWidgetTransform Transform;
	Transform.Translation = FVector2D(ShakeOffset, -SelectedLift * LiftAlpha);
	Transform.Scale = FVector2D(FMath::Lerp(1.0f, SelectedScale, LiftAlpha));
	SetRenderTransform(Transform);
}

FReply UDRStageSelectCardWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 커서가 실제로 움직였을 때만 선택이 따라온다. Slate 는 멈춘 커서 아래에 위젯이 새로 생겨도 변위 0 인
	// 합성 이동(Enter/Move)을 보내므로, 그것까지 받으면 화면을 열자마자 기본 선택(새로 열린 스테이지)이 덮어써진다
	if (!bSelected && !InMouseEvent.GetCursorDelta().IsNearlyZero())
	{
		if (UDRStageSelectScreenWidget* Screen = OwnerScreen.Get())
		{
			Screen->NotifyCardHovered(ViewModel.Index);
		}
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
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
