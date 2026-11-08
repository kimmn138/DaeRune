// Copyright DaeRune


#include "UI/Widget/DRInteractionPromptWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

namespace DRInteractionPrompt
{
	// 높이를 정하고 가로는 그림 비율대로
	FVector2D SizeForHeight(UTexture2D* Texture, float Height)
	{
#if WITH_EDITOR
		// 에디터는 텍스처를 비동기로 컴파일한다 — 끝나기 전엔 크기가 자리표시 그림 크기로 나온다 (스테이지 선택 카드와 같다)
		if (Texture->IsCompiling())
		{
			FTextureCompilingManager::Get().FinishCompilation({ Texture });
		}
#endif
		const float SurfaceHeight = Texture->GetSurfaceHeight();
		return FVector2D(SurfaceHeight > 0.f ? Texture->GetSurfaceWidth() * Height / SurfaceHeight : Height, Height);
	}

	void SetImage(UImage* Image, UTexture2D* Texture, float Height)
	{
		FSlateBrush Brush = Image->GetBrush();
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = SizeForHeight(Texture, Height);
		Image->SetBrush(Brush);
	}
}

void UDRInteractionPromptWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너에서 트리를 손으로 고쳐 바인딩이 빠졌을 때 (게임에서는 BindWidget 이 컴파일에서 막는다)
	if (!Panel_Prompt || !Panel_Key || !Image_Key || !Panel_KeyCap || !Text_Key || !Text_Prompt
		|| !Panel_CarriedPart || !Image_CarriedPart || !Panel_PartPlaceholder)
	{
		return;
	}

	if (IsDesignTime())
	{
		// 디자이너 미리보기 — 클래스 기본값에 넣은 그림이 어떻게 보이는지 (문구는 트리의 미리보기 글자)
		ApplyKey(PreviewKey);
		ApplyCarriedPartIcon();
		return;
	}

	// 게임에서는 감지 컴포넌트가 SetPrompt 하기 전까지 아무것도 띄우지 않는다
	CurrentPrompt = FDRInteractionPrompt();
	HidePanel(Panel_Prompt, PromptFadeElapsed);
	HidePanel(Panel_CarriedPart, PartFadeElapsed);
	ApplyCarriedPartIcon();
}

void UDRInteractionPromptWidget::SetPrompt(const FDRInteractionPrompt& Prompt)
{
	if (Prompt.IsSameAs(CurrentPrompt)) return;

	CurrentPrompt = Prompt;

	if (Prompt.HasText())
	{
		Text_Prompt->SetText(Prompt.Text);
		ApplyKey(Prompt.Key);
		ShowPanel(Panel_Prompt, PromptFadeElapsed);
	}
	else
	{
		HidePanel(Panel_Prompt, PromptFadeElapsed);
	}

	if (Prompt.bShowCarriedPart)
	{
		ShowPanel(Panel_CarriedPart, PartFadeElapsed);
	}
	else
	{
		HidePanel(Panel_CarriedPart, PartFadeElapsed);
	}
}

void UDRInteractionPromptWidget::ApplyKey(const FKey& Key)
{
	// 키 없는 안내(지금은 쓸 수 없다는 문구)면 키 자리를 통째로 접는다 — 간격도 같이 사라져 문구가 가운데로 온다
	if (!Key.IsValid())
	{
		Panel_Key->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Panel_Key->SetVisibility(ESlateVisibility::HitTestInvisible);

	const TObjectPtr<UTexture2D>* Icon = KeyIcons.Find(Key);
	if (Icon && *Icon)
	{
		DRInteractionPrompt::SetImage(Image_Key, *Icon, KeyIconHeight);
		Image_Key->SetVisibility(ESlateVisibility::HitTestInvisible);
		Panel_KeyCap->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		// 그림이 아직 없는 키 — 키 이름("F" · "Space")을 둥근 키캡에 쓴다
		Text_Key->SetText(Key.GetDisplayName(false));
		Image_Key->SetVisibility(ESlateVisibility::Collapsed);
		Panel_KeyCap->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UDRInteractionPromptWidget::ApplyCarriedPartIcon()
{
	if (CarriedPartIcon)
	{
		DRInteractionPrompt::SetImage(Image_CarriedPart, CarriedPartIcon, CarriedPartIconHeight);
		Image_CarriedPart->SetVisibility(ESlateVisibility::HitTestInvisible);
		Panel_PartPlaceholder->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		Image_CarriedPart->SetVisibility(ESlateVisibility::Collapsed);
		Panel_PartPlaceholder->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UDRInteractionPromptWidget::ShowPanel(UWidget* Panel, float& FadeElapsed)
{
	if (Panel->GetVisibility() != ESlateVisibility::Collapsed) return;

	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (FadeInDuration > 0.f)
	{
		FadeElapsed = 0.f;
		Panel->SetRenderOpacity(0.f);
	}
}

void UDRInteractionPromptWidget::HidePanel(UWidget* Panel, float& FadeElapsed)
{
	FadeElapsed = -1.f;
	Panel->SetRenderOpacity(1.f);
	Panel->SetVisibility(ESlateVisibility::Collapsed);
}

void UDRInteractionPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	auto TickFade = [this, InDeltaTime](UWidget* Panel, float& FadeElapsed)
	{
		if (FadeElapsed < 0.f) return;

		FadeElapsed += InDeltaTime;
		const float Alpha = FadeInDuration > 0.f ? FMath::Clamp(FadeElapsed / FadeInDuration, 0.f, 1.f) : 1.f;
		Panel->SetRenderOpacity(Alpha);
		if (Alpha >= 1.f)
		{
			FadeElapsed = -1.f;
		}
	};

	TickFade(Panel_Prompt, PromptFadeElapsed);
	TickFade(Panel_CarriedPart, PartFadeElapsed);
}
