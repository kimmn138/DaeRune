// Copyright DaeRune


#include "DRInteractionPromptEditorTools.h"

#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "DaeRune/DRLogChannels.h"
#include "DREditorToolCommon.h"
#endif

#if WITH_EDITOR
namespace DRInteractionPromptEditor
{
	// 위젯 도구 공용 헬퍼 (DREditorToolCommon.h)
	using DREditorToolCommon::SRGB;
	using DREditorToolCommon::PickTypeface;
	using DREditorToolCommon::DiscardRecursive;
	using DREditorToolCommon::Load;
	using DREditorToolCommon::CompileAndMark;

	// ===== 에셋 경로 =====
	const TCHAR* PromptWBPPath = TEXT("/Game/Blueprints/UI/Interaction/WBP_InteractionPrompt.WBP_InteractionPrompt");
	const TCHAR* HakgyoansimFontPath = TEXT("/Game/DaeRuneAssets/UI/Font/HakgyoansimUndongjangL_Font.HakgyoansimUndongjangL_Font");

	struct FLog
	{
		FString& Out;
		void Add(const FString& Line)
		{
			Out += Line + TEXT("\n");
			UE_LOG(LogDR, Display, TEXT("[InteractionPromptEditor] %s"), *Line);
		}
	};

	// 시안의 글자 · 키캡 색 #94D2DB
	const FLinearColor PromptColor = SRGB(148, 210, 219);

	// ===== Spec — 시안 캡처(1959x1105)를 1080 높이로 환산한 값 (UI 는 짧은 변 기준으로 배율이 붙는다) =====
	namespace Spec
	{
		// 안내 줄 · 부품 아이콘의 세로 가운데 — 화면 가운데(조준선)에서 아래로
		const float PromptCenterY = 254.6f;
		const float PartIconCenterY = 333.3f;

		// 문구 — 학교안심 운동장 22.7px (시안 글자 잉크 폭 156 · 높이 20.5 로 맞췄다)
		const float PromptPx = 22.7f;

		// 키캡 오른쪽 끝 → 글자 상자 (잉크 간격 16.6 - 첫 글자 왼쪽 여백 1.1)
		const float KeyToTextGap = 15.5f;

		// 키캡 — 26x26, 모서리 4.5, 테두리 1.5 (시안 픽셀로 잰 값)
		const float KeyCapSize = 26.0f;
		const float KeyCapRadius = 4.5f;
		const float KeyCapOutline = 1.5f;

		// 키캡 · 부품 틀 안쪽 — 시안은 어두운 장면 위라 시안색 10% 채움이었지만, 로비의 밝은 하늘색 벽 위에서는
		// 테두리 · 글자가 사라졌다 → 어두운 청록 반투명으로 깐다 (어두운 장면에서는 시안과 거의 같다)
		const FLinearColor BoxFill = SRGB(16, 36, 52, 0.45f);

		// 키 글자 — 학교안심 18.7px (F 잉크 높이 12.7). F 잉크가 줄 상자 가운데보다 0.85 위라 그만큼 내린다
		// (★가운데 정렬의 여백은 절반이 아니라 차이만큼 민다★). 좌우 6 은 "Space" 처럼 긴 키 이름의 여백
		const float KeyPx = 18.7f;
		const FMargin KeyCapPadding(6.0f, 0.85f, 6.0f, 0.0f);

		// 부품 아이콘 그림이 없을 때의 빈 틀 — 시안 아이콘 틀 크기 94x74
		const FVector2D PartSlotSize(94.0f, 74.0f);
		const float PartPlaceholderRadius = 8.0f;
		const float PartPlaceholderPx = 18.0f;

		// 문구 외곽선 · 그림자 — 시안에는 없지만 밝은 바닥 · 벽 위에서도 읽히게 깐다 (어두운 장면에서는 거의 안 보인다)
		const int32 TextOutlineSize = 1;
		const FLinearColor TextOutlineColor = SRGB(8, 20, 36, 0.7f);
		const FVector2D TextShadowOffset(0.0f, 1.5f);
		const FLinearColor TextShadowColor(0.0f, 0.0f, 0.0f, 0.4f);
	}

	// Px 는 시안 px. UMG 폰트 크기는 72dpi 포인트라 ×0.75
	FSlateFontInfo MakeFont(const UObject* FontObject, float Px)
	{
		return FSlateFontInfo(FontObject, Px * 0.75f, PickTypeface(FontObject));
	}

	// 둥근 테두리 상자 (채움 + 테두리선)
	FSlateBrush MakeRoundedOutlineBrush(const FLinearColor& Fill, const FLinearColor& Outline, float Radius, float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), OutlineWidth);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	// 트리를 비우고 새 루트를 만든다 — 멱등을 위해 매번 처음부터 짓는다
	template <typename T>
	T* ResetTree(UWidgetBlueprint* Blueprint, const TCHAR* RootName)
	{
		UWidgetTree* Tree = Blueprint->WidgetTree;
		DiscardRecursive(Tree, Tree->RootWidget);
		Tree->RootWidget = nullptr;

		// 이름 충돌 방지 — 루트 밖에 떠 있던 위젯까지 정리
		TArray<UWidget*> Leftovers;
		Tree->GetAllWidgets(Leftovers);
		for (UWidget* Leftover : Leftovers)
		{
			DiscardRecursive(Tree, Leftover);
		}

		T* Root = Tree->ConstructWidget<T>(T::StaticClass(), RootName);
		Tree->RootWidget = Root;
		return Root;
	}

	template <typename T>
	T* Make(UWidgetTree* Tree, const TCHAR* Name, bool bVariable = false)
	{
		T* Widget = Tree->ConstructWidget<T>(T::StaticClass(), Name);
		Widget->bIsVariable = bVariable;
		Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Widget;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const TCHAR* Name, const FText& Text, const UObject* Font, float Px, bool bVariable)
	{
		UTextBlock* TextBlock = Make<UTextBlock>(Tree, Name, bVariable);
		TextBlock->SetText(Text);
		TextBlock->SetFont(MakeFont(Font, Px));
		TextBlock->SetColorAndOpacity(FSlateColor(PromptColor));
		return TextBlock;
	}

	UOverlaySlot* AddToOverlay(UOverlay* Parent, UWidget* Child, EHorizontalAlignment H, EVerticalAlignment V)
	{
		UOverlaySlot* OverlaySlot = Parent->AddChildToOverlay(Child);
		OverlaySlot->SetHorizontalAlignment(H);
		OverlaySlot->SetVerticalAlignment(V);
		return OverlaySlot;
	}

	UHorizontalBoxSlot* AddToHBox(UHorizontalBox* Parent, UWidget* Child, const FMargin& Padding = FMargin(0.0f))
	{
		UHorizontalBoxSlot* BoxSlot = Parent->AddChildToHorizontalBox(Child);
		BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		BoxSlot->SetHorizontalAlignment(HAlign_Left);
		BoxSlot->SetVerticalAlignment(VAlign_Center);
		BoxSlot->SetPadding(Padding);
		return BoxSlot;
	}

	// 화면 가운데 기준 점에 자동 크기로 — 자기 가운데를 그 점에 맞춘다
	UCanvasPanelSlot* AddToCanvasCentered(UCanvasPanel* Canvas, UWidget* Child, const FVector2D& OffsetFromCenter)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Child);
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.5, 0.5));
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetOffsets(FMargin(OffsetFromCenter.X, OffsetFromCenter.Y, 0.0f, 0.0f));
		return CanvasSlot;
	}
}
#endif

bool UDRInteractionPromptEditorTools::BuildInteractionPromptWidget(FString& OutLog)
{
#if WITH_EDITOR
	using namespace DRInteractionPromptEditor;
	OutLog.Reset();
	FLog Log{ OutLog };

	UWidgetBlueprint* Blueprint = Load<UWidgetBlueprint>(PromptWBPPath, Log);
	UObject* Hakgyoansim = Load<UObject>(HakgyoansimFontPath, Log);
	if (!Blueprint || !Blueprint->WidgetTree) return false;

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();

	// 1920x1080 기준 전체 화면 캔버스 — 클릭은 하나도 받지 않는다 (게임 입력이 그대로 지나간다)
	UCanvasPanel* Root = ResetTree<UCanvasPanel>(Blueprint, TEXT("CanvasPanel_Root"));
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 1) [키][문구] 한 줄 — 묶음째 가운데, 조준선 아래
	UHorizontalBox* PromptRow = Make<UHorizontalBox>(Tree, TEXT("Panel_Prompt"), true);
	AddToCanvasCentered(Root, PromptRow, FVector2D(0.0, Spec::PromptCenterY));
	{
		// 키 자리 — 그림(Image_Key)과 글자 키캡(Panel_KeyCap) 중 하나만 보인다.
		// 문구와의 간격은 이 슬롯의 오른쪽 여백이라, 키 없는 안내에서 자리를 접으면 간격도 같이 사라진다
		UOverlay* KeySlot = Make<UOverlay>(Tree, TEXT("Panel_Key"), true);
		AddToHBox(PromptRow, KeySlot, FMargin(0.0f, 0.0f, Spec::KeyToTextGap, 0.0f));

		UImage* KeyImage = Make<UImage>(Tree, TEXT("Image_Key"), true);
		KeyImage->SetVisibility(ESlateVisibility::Collapsed);
		AddToOverlay(KeySlot, KeyImage, HAlign_Center, VAlign_Center);

		USizeBox* KeyCap = Make<USizeBox>(Tree, TEXT("Panel_KeyCap"), true);
		KeyCap->SetMinDesiredWidth(Spec::KeyCapSize);
		KeyCap->SetHeightOverride(Spec::KeyCapSize);
		AddToOverlay(KeySlot, KeyCap, HAlign_Center, VAlign_Center);

		UBorder* KeyBorder = Make<UBorder>(Tree, TEXT("Border_KeyCap"));
		KeyBorder->SetBrush(MakeRoundedOutlineBrush(Spec::BoxFill, PromptColor, Spec::KeyCapRadius, Spec::KeyCapOutline));
		KeyBorder->SetPadding(Spec::KeyCapPadding);
		KeyBorder->SetHorizontalAlignment(HAlign_Center);
		KeyBorder->SetVerticalAlignment(VAlign_Center);
		KeyCap->SetContent(KeyBorder);

		// 키 이름 · 문구는 C++ 이 매번 넣는다 — 미리보기 글자는 번역 수집에서 빠지도록 CultureInvariant
		KeyBorder->SetContent(MakeText(Tree, TEXT("Text_Key"), FText::AsCultureInvariant(TEXT("F")), Hakgyoansim, Spec::KeyPx, true));

		UTextBlock* PromptText = MakeText(Tree, TEXT("Text_Prompt"), FText::AsCultureInvariant(TEXT("부품 떨어뜨리기")), Hakgyoansim, Spec::PromptPx, true);
		FSlateFontInfo PromptFont = PromptText->GetFont();
		PromptFont.OutlineSettings.OutlineSize = Spec::TextOutlineSize;
		PromptFont.OutlineSettings.OutlineColor = Spec::TextOutlineColor;
		PromptText->SetFont(PromptFont);
		PromptText->SetShadowOffset(Spec::TextShadowOffset);
		PromptText->SetShadowColorAndOpacity(Spec::TextShadowColor);
		AddToHBox(PromptRow, PromptText);
	}

	// 2) 들고 있는 부품 아이콘 — 그림(Image_CarriedPart)이 없으면 빈 틀(Panel_PartPlaceholder)
	UOverlay* PartSlot = Make<UOverlay>(Tree, TEXT("Panel_CarriedPart"), true);
	AddToCanvasCentered(Root, PartSlot, FVector2D(0.0, Spec::PartIconCenterY));
	{
		UImage* PartImage = Make<UImage>(Tree, TEXT("Image_CarriedPart"), true);
		PartImage->SetVisibility(ESlateVisibility::Collapsed);
		AddToOverlay(PartSlot, PartImage, HAlign_Center, VAlign_Center);

		USizeBox* Placeholder = Make<USizeBox>(Tree, TEXT("Panel_PartPlaceholder"), true);
		Placeholder->SetWidthOverride(Spec::PartSlotSize.X);
		Placeholder->SetHeightOverride(Spec::PartSlotSize.Y);
		AddToOverlay(PartSlot, Placeholder, HAlign_Center, VAlign_Center);

		UBorder* PlaceholderBorder = Make<UBorder>(Tree, TEXT("Border_PartPlaceholder"));
		PlaceholderBorder->SetBrush(MakeRoundedOutlineBrush(Spec::BoxFill, PromptColor, Spec::PartPlaceholderRadius, Spec::KeyCapOutline));
		PlaceholderBorder->SetHorizontalAlignment(HAlign_Center);
		PlaceholderBorder->SetVerticalAlignment(VAlign_Center);
		Placeholder->SetContent(PlaceholderBorder);

		// 부품 아이콘 그림(WBP 클래스 기본값 CarriedPartIcon)을 넣기 전까지 게임에도 보이는 글자 — 번역 대상
		PlaceholderBorder->SetContent(MakeText(Tree, TEXT("Text_PartPlaceholder"),
			NSLOCTEXT("DRInteraction", "CarriedPartPlaceholder", "부품"), Hakgyoansim, Spec::PartPlaceholderPx, false));
	}

	return CompileAndMark(Blueprint, Log);
#else
	return false;
#endif
}
