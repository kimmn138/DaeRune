// Copyright DaeRune


#include "DRStageSelectEditorTools.h"

#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/BackgroundBlur.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Materials/MaterialInterface.h"
#include "DaeRune/DRLogChannels.h"
#include "DREditorToolCommon.h"
#endif

#if WITH_EDITOR
namespace DRStageSelectEditor
{
	// 세 도구 공용 헬퍼 (DREditorToolCommon.h)
	using DREditorToolCommon::SRGB;
	using DREditorToolCommon::PickTypeface;
	using DREditorToolCommon::DiscardRecursive;
	using DREditorToolCommon::Load;
	using DREditorToolCommon::CompileAndMark;
	using DREditorToolCommon::SetObjectProperty;

	// ===== 에셋 경로 =====
	const TCHAR* CardWBPPath = TEXT("/Game/Blueprints/UI/StageSelect/WBP_StageSelectCard.WBP_StageSelectCard");
	const TCHAR* ScreenWBPPath = TEXT("/Game/Blueprints/UI/StageSelect/WBP_StageSelect.WBP_StageSelect");
	const TCHAR* LockBlendMaterialPath = TEXT("/Game/Blueprints/UI/StageSelect/Material/M_UI_StageSelect_LockBlend.M_UI_StageSelect_LockBlend");

	// 카드 미리보기 브러시 (스테이지 1) — 런타임에는 카드 위젯이 스테이지 데이터(CardArt)로 갈아 끼운다
	const TCHAR* PreviewBasePath = TEXT("/Game/DaeRuneAssets/UI/StageSelect/Stage1/T_StageSelect_S1_Base.T_StageSelect_S1_Base");
	const TCHAR* PreviewPicturePath = TEXT("/Game/DaeRuneAssets/UI/StageSelect/Stage1/T_StageSelect_S1_Picture.T_StageSelect_S1_Picture");
	const TCHAR* PreviewOutlinePath = TEXT("/Game/DaeRuneAssets/UI/StageSelect/Stage1/T_StageSelect_S1_Outline.T_StageSelect_S1_Outline");
	const TCHAR* PreviewNumberPath = TEXT("/Game/DaeRuneAssets/UI/StageSelect/Stage1/T_StageSelect_S1_Number.T_StageSelect_S1_Number");
	const TCHAR* PreviewBadgePath = TEXT("/Game/DaeRuneAssets/UI/StageSelect/Stage1/T_StageSelect_S1_Badge.T_StageSelect_S1_Badge");
	const TCHAR* LockIconPath = TEXT("/Game/DaeRuneAssets/UI/StageSelect/T_StageSelect_Lock.T_StageSelect_Lock");
	const TCHAR* ClearedStampPath = TEXT("/Game/DaeRuneAssets/UI/StageSelect/T_StageSelect_Cleared.T_StageSelect_Cleared");
	const TCHAR* KeyEscPath = TEXT("/Game/DaeRuneAssets/UI/Settimgs/T_Key_ESC.T_Key_ESC");
	const TCHAR* KeyEnterPath = TEXT("/Game/DaeRuneAssets/UI/Settimgs/T_Key_ENTER.T_Key_ENTER");

	// 스테이지 이름 · "스테이지 N" · 제목 · 키 안내는 학교안심 운동장, 임무 설명은 Pretendard (Figma 글꼴을 렌더링해 맞춰 봤다)
	const TCHAR* HakgyoansimFontPath = TEXT("/Game/DaeRuneAssets/UI/Font/HakgyoansimUndongjangL_Font.HakgyoansimUndongjangL_Font");
	const TCHAR* PretendardFontPath = TEXT("/Game/DaeRuneAssets/UI/Font/Pretendard-SemiBold__1__Font.Pretendard-SemiBold__1__Font");

	struct FLog
	{
		FString& Out;
		void Add(const FString& Line)
		{
			Out += Line + TEXT("\n");
			UE_LOG(LogDR, Display, TEXT("[StageSelectEditor] %s"), *Line);
		}
	};

	// Figma 글자색 #DDF7FF (모든 글자 · 번호 그림이 같은 색)
	const FLinearColor TextColor = SRGB(0xDD, 0xF7, 0xFF);

	// ===== Spec — Figma 프레임 "Slide 16:9 - 7 / 8" 을 100% 로 떠서 잰 값 (1920x1080 기준 px) =====
	namespace Spec
	{
		// 카드 레이어 위치 (카드 모양 왼쪽 위 기준, 원래 크기)
		const FVector2D GlowLayerOffset(-50.0, -50.0);	// 베이스 · 테두리 = 글로우 캔버스 (사방 50px)
		const FVector2D PictureOffset(-13.0, -1.0);		// "스테이지N 그림" 내보내기 프레임 자리

		// 번호 그림 — 아래쪽 기준 (두 카드의 높이가 달라 위 기준으로는 안 맞는다). 1번: 카드 (38,58) 에 72x70
		const FMargin NumberPadding(38.0f, 0.0f, 0.0f, 542.0f);
		const float NumberHeight = 70.0f;

		// 이름 · 배지 · 임무 묶음 — 아래쪽 기준. 글꼴 줄 높이(학교안심 1.0em · Pretendard 1.19em)로 잉크 위치를 맞췄다
		const FMargin TextBlockPadding(34.0f, 0.0f, 0.0f, 48.6f);
		const float BadgeGap = 18.3f;				// 이름 글자 상자 끝 → 배지
		const float MissionGap = 23.5f;				// 이름 줄 → 임무 줄
		const float TitlePx = 40.0f;				// "연구 격납고" 학교안심 40px
		const float BadgePx = 24.0f;				// "스테이지 1" 학교안심 24px
		const float MissionPx = 24.5f;				// 임무 Pretendard 24.5px (SemiBold 기준 폭 맞춤)

		// CLEARED 도장 — 카드 가운데에서 (+5,-1), 475x165, 반시계 11도.
		// ★Slate 가운데 정렬은 여백 차(왼-오른, 위-아래)만큼 그대로 민다★ (절반이 아니다 — LayoutUtils AlignChild)
		const FMargin StampPadding(5.0f, 0.0f, 0.0f, 1.0f);
		const FVector2D StampSize(475.0, 165.0);
		const float StampAngle = -11.0f;

		// 자물쇠 — 오른쪽 위, 원래 크기(125x125, 글로우 포함). 아래쪽 기준으로 두 카드 공통
		const FMargin LockPadding(0.0f, 0.0f, 24.0f, 517.0f);

		// 화면
		const float ScreenTitleTop = 86.9f;			// "스테이지 선택" 학교안심 55px — 잉크 위 90
		const float ScreenTitlePx = 55.0f;
		const float CardsTop = 223.0f;				// 1번 카드 위(계단 포함). 2번은 아래쪽 맞춤으로 232 에 온다

		// 키 안내 — 오른쪽 아래. 버튼 안쪽 여백(8,4) 포함한 묶음의 오른쪽 끝 · 세로 가운데
		const FVector2D KeyHintsAnchorOffset(-73.2, -94.5);
		const FMargin KeyButtonPadding(8.0f, 4.0f);
		const float KeyIconHeight = 31.0f;			// ESC 57x31 · ENTER 85x31
		const float KeyLabelGapBack = 9.0f;			// ESC 아이콘 → BACK
		const float KeyLabelGapStart = 10.0f;		// ENTER 아이콘 → START
		const float KeyButtonGap = 14.6f;			// BACK 버튼 → START 버튼
		const float BackPx = 23.0f;					// "BACK" 학교안심 23px
		const float StartPx = 29.0f;				// "START" 학교안심 29px (시안에서 BACK 보다 크다)

		// 배경 — 시안의 어두운 복도 톤에 맞춰 로비 화면을 어둡게 덮는다
		const FLinearColor DimColor = SRGB(6, 10, 20, 0.72f);
		const float BlurStrength = 3.0f;
	}

	// Px 는 시안 px. UMG 폰트 크기는 72dpi 포인트라 ×0.75
	FSlateFontInfo MakeFont(const UObject* FontObject, float Px)
	{
		return FSlateFontInfo(FontObject, Px * 0.75f, PickTypeface(FontObject));
	}

	FSlateBrush MakeTextureBrush(UTexture2D* Texture, FVector2D Size = FVector2D::ZeroVector)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		if (Texture)
		{
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = Size.IsNearlyZero() ? FVector2D(Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight()) : Size;
		}
		return Brush;
	}

	// 높이를 정하고 가로는 그림 비율대로
	FVector2D SizeForHeight(const UTexture2D* Texture, float Height)
	{
		if (!Texture || Texture->GetSurfaceHeight() <= 0.0f) return FVector2D(Height, Height);
		return FVector2D(Texture->GetSurfaceWidth() * Height / Texture->GetSurfaceHeight(), Height);
	}

	FSlateBrush MakeColorBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

	FSlateBrush MakeRoundedBrush(const FLinearColor& Fill, float Radius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(FLinearColor::Transparent), 0.0f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	FSlateBrush MakeNoDrawBrush()
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
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
		return Widget;
	}

	UImage* MakeImage(UWidgetTree* Tree, const TCHAR* Name, UTexture2D* Texture, bool bVariable, FVector2D Size = FVector2D::ZeroVector)
	{
		UImage* Image = Make<UImage>(Tree, Name, bVariable);
		Image->SetBrush(MakeTextureBrush(Texture, Size));
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Image;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const TCHAR* Name, const FText& Text, const UObject* Font, float Px, bool bVariable)
	{
		UTextBlock* TextBlock = Make<UTextBlock>(Tree, Name, bVariable);
		TextBlock->SetText(Text);
		TextBlock->SetFont(MakeFont(Font, Px));
		TextBlock->SetColorAndOpacity(FSlateColor(TextColor));
		TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		return TextBlock;
	}

	UOverlaySlot* AddToOverlay(UOverlay* Parent, UWidget* Child, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Padding = FMargin(0.0f))
	{
		UOverlaySlot* OverlaySlot = Parent->AddChildToOverlay(Child);
		OverlaySlot->SetHorizontalAlignment(H);
		OverlaySlot->SetVerticalAlignment(V);
		OverlaySlot->SetPadding(Padding);
		return OverlaySlot;
	}

	UVerticalBoxSlot* AddToVBox(UVerticalBox* Parent, UWidget* Child, EHorizontalAlignment H, const FMargin& Padding = FMargin(0.0f))
	{
		UVerticalBoxSlot* BoxSlot = Parent->AddChildToVerticalBox(Child);
		BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		BoxSlot->SetHorizontalAlignment(H);
		BoxSlot->SetVerticalAlignment(VAlign_Top);
		BoxSlot->SetPadding(Padding);
		return BoxSlot;
	}

	UHorizontalBoxSlot* AddToHBox(UHorizontalBox* Parent, UWidget* Child, EVerticalAlignment V, const FMargin& Padding = FMargin(0.0f))
	{
		UHorizontalBoxSlot* BoxSlot = Parent->AddChildToHorizontalBox(Child);
		BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		BoxSlot->SetHorizontalAlignment(HAlign_Left);
		BoxSlot->SetVerticalAlignment(V);
		BoxSlot->SetPadding(Padding);
		return BoxSlot;
	}

	UCanvasPanelSlot* AddToCanvas(UCanvasPanel* Canvas, UWidget* Child, const FAnchors& Anchors, const FVector2D& Alignment,
		const FVector2D& Position, bool bAutoSize, const FVector2D& Size = FVector2D::ZeroVector)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Child);
		CanvasSlot->SetAnchors(Anchors);
		CanvasSlot->SetAlignment(Alignment);
		CanvasSlot->SetAutoSize(bAutoSize);
		// 앵커가 한 점이면 (X, Y, 폭, 높이), 늘이기면 (왼, 위, 오른, 아래 여백)
		CanvasSlot->SetOffsets(FMargin(Position.X, Position.Y, Size.X, Size.Y));
		return CanvasSlot;
	}

	// 키 안내 버튼 — 배경은 안 그리고 마우스를 올렸을 때만 옅게 밝힌다. 키 입력은 화면이 받으므로 포커스는 받지 않는다
	void StyleKeyHintButton(UButton* Button)
	{
		FButtonStyle Style = Button->GetStyle();
		Style.SetNormal(MakeNoDrawBrush())
			.SetHovered(MakeRoundedBrush(SRGB(143, 227, 255, 0.12f), 6.0f))
			.SetPressed(MakeRoundedBrush(SRGB(143, 227, 255, 0.22f), 6.0f))
			.SetDisabled(MakeNoDrawBrush());
		Style.SetNormalPadding(Spec::KeyButtonPadding).SetPressedPadding(Spec::KeyButtonPadding);
		Button->SetStyle(Style);

		// (InitIsFocusable 은 protected · 멤버 직접 접근은 deprecated — 디자이너에서 체크를 끄는 것과 같게 반사로 넣는다)
		if (FBoolProperty* FocusableProperty = FindFProperty<FBoolProperty>(UButton::StaticClass(), TEXT("IsFocusable")))
		{
			FocusableProperty->SetPropertyValue_InContainer(Button, false);
		}
		Button->bIsVariable = true;
	}
}
#endif

bool UDRStageSelectEditorTools::BuildStageSelectCardWidget(FString& OutLog)
{
#if WITH_EDITOR
	using namespace DRStageSelectEditor;
	OutLog.Reset();
	FLog Log{ OutLog };

	UWidgetBlueprint* Blueprint = Load<UWidgetBlueprint>(CardWBPPath, Log);
	UMaterialInterface* LockBlendMaterial = Load<UMaterialInterface>(LockBlendMaterialPath, Log);
	UTexture2D* BaseTexture = Load<UTexture2D>(PreviewBasePath, Log);
	UTexture2D* PictureTexture = Load<UTexture2D>(PreviewPicturePath, Log);
	UTexture2D* OutlineTexture = Load<UTexture2D>(PreviewOutlinePath, Log);
	UTexture2D* NumberTexture = Load<UTexture2D>(PreviewNumberPath, Log);
	UTexture2D* BadgeTexture = Load<UTexture2D>(PreviewBadgePath, Log);
	UTexture2D* LockTexture = Load<UTexture2D>(LockIconPath, Log);
	UTexture2D* StampTexture = Load<UTexture2D>(ClearedStampPath, Log);
	UObject* Hakgyoansim = Load<UObject>(HakgyoansimFontPath, Log);
	UObject* Pretendard = Load<UObject>(PretendardFontPath, Log);
	if (!Blueprint || !Blueprint->WidgetTree) return false;

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();

	// 루트 = 카드 모양 크기 (런타임에 CardArt.CardSize 로 바뀐다). 글로우는 이 밖으로 그린다
	USizeBox* Root = ResetTree<USizeBox>(Blueprint, TEXT("SizeBox_Card"));
	Root->bIsVariable = true;
	Root->SetWidthOverride(787.0f);
	Root->SetHeightOverride(670.0f);

	// Overlay 는 뒤에 넣은 것이 위에 그려진다 — 레이어가이드 순서(아래 → 위)
	UOverlay* Card = Make<UOverlay>(Tree, TEXT("Overlay_Card"));
	Card->SetVisibility(ESlateVisibility::HitTestInvisible);
	Root->SetContent(Card);

	// 베이스 · 그림 · 테두리는 카드 밖으로 넘치는 그림이라 캔버스에 원래 크기로 둔다.
	// ★Overlay 에 두면 안 된다★ — 왼/위 정렬이어도 남은 공간(카드 크기 - 여백)으로 줄여 그려서 카드가 6% 작아진다
	UCanvasPanel* Layers = Make<UCanvasPanel>(Tree, TEXT("Canvas_Layers"));
	Layers->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToOverlay(Card, Layers, HAlign_Fill, VAlign_Fill);
	{
		const FAnchors TopLeft(0.0f, 0.0f);
		AddToCanvas(Layers, MakeImage(Tree, TEXT("Image_Base"), BaseTexture, true), TopLeft, FVector2D::ZeroVector, Spec::GlowLayerOffset, true);
		AddToCanvas(Layers, MakeImage(Tree, TEXT("Image_Thumbnail"), PictureTexture, true), TopLeft, FVector2D::ZeroVector, Spec::PictureOffset, true);
		AddToCanvas(Layers, MakeImage(Tree, TEXT("Image_Frame"), OutlineTexture, true), TopLeft, FVector2D::ZeroVector, Spec::GlowLayerOffset, true);
	}

	// CLEARED 도장 — 클리어일 때 C++ 이 켠다
	UImage* Stamp = MakeImage(Tree, TEXT("Panel_Cleared"), StampTexture, true, Spec::StampSize);
	Stamp->SetRenderTransformAngle(Spec::StampAngle);
	Stamp->SetVisibility(ESlateVisibility::Collapsed);
	AddToOverlay(Card, Stamp, HAlign_Center, VAlign_Center, Spec::StampPadding);

	// 번호 + 글자 묶음 — 카드마다 ContentOffset 만큼 옮기고, 잠기면 묶음째 40%
	UOverlay* Content = Make<UOverlay>(Tree, TEXT("Panel_Content"), true);
	Content->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToOverlay(Card, Content, HAlign_Fill, VAlign_Fill);
	{
		UImage* Number = MakeImage(Tree, TEXT("Image_Number"), NumberTexture, true, SizeForHeight(NumberTexture, Spec::NumberHeight));
		Number->SetRenderOpacity(0.7f);
		AddToOverlay(Content, Number, HAlign_Left, VAlign_Bottom, Spec::NumberPadding);

		UVerticalBox* TextBlock = Make<UVerticalBox>(Tree, TEXT("VerticalBox_Text"));
		TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddToOverlay(Content, TextBlock, HAlign_Left, VAlign_Bottom, Spec::TextBlockPadding);

		UHorizontalBox* NameRow = Make<UHorizontalBox>(Tree, TEXT("HorizontalBox_Name"));
		NameRow->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddToVBox(TextBlock, NameRow, HAlign_Left);

		// 런타임 글자는 C++ 이 넣는다 — 미리보기 글자는 번역 수집에서 빠지도록 CultureInvariant
		AddToHBox(NameRow, MakeText(Tree, TEXT("Txt_Title"), FText::AsCultureInvariant(TEXT("연구 격납고")), Hakgyoansim, Spec::TitlePx, true), VAlign_Center);

		UOverlay* Badge = Make<UOverlay>(Tree, TEXT("Overlay_Badge"));
		Badge->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddToHBox(NameRow, Badge, VAlign_Center, FMargin(Spec::BadgeGap, 0.0f, 0.0f, 0.0f));
		AddToOverlay(Badge, MakeImage(Tree, TEXT("Image_Badge"), BadgeTexture, true), HAlign_Fill, VAlign_Fill);
		AddToOverlay(Badge, MakeText(Tree, TEXT("Txt_Badge"), FText::AsCultureInvariant(TEXT("스테이지 1")), Hakgyoansim, Spec::BadgePx, true), HAlign_Center, VAlign_Center);

		AddToVBox(TextBlock, MakeText(Tree, TEXT("Txt_Mission"), FText::AsCultureInvariant(TEXT("적들로부터 클렌저를 지키세요")), Pretendard, Spec::MissionPx, true),
			HAlign_Left, FMargin(0.0f, Spec::MissionGap, 0.0f, 0.0f));
	}

	// 자물쇠 — 잠겼을 때 C++ 이 켠다 (잠금 효과 위에 그려지므로 컬러 그대로)
	UImage* Lock = MakeImage(Tree, TEXT("Panel_Locked"), LockTexture, true);
	Lock->SetVisibility(ESlateVisibility::Collapsed);
	AddToOverlay(Card, Lock, HAlign_Right, VAlign_Bottom, Spec::LockPadding);

	if (!CompileAndMark(Blueprint, Log)) return false;

	SetObjectProperty(Blueprint->GeneratedClass->GetDefaultObject(), TEXT("LockBlendMaterial"), LockBlendMaterial, Log);
	return CompileAndMark(Blueprint, Log);	// CDO 를 고쳤으면 다시 컴파일 (옷장 T-32)
#else
	return false;
#endif
}

bool UDRStageSelectEditorTools::BuildStageSelectScreenWidget(FString& OutLog)
{
#if WITH_EDITOR
	using namespace DRStageSelectEditor;
	OutLog.Reset();
	FLog Log{ OutLog };

	UWidgetBlueprint* Blueprint = Load<UWidgetBlueprint>(ScreenWBPPath, Log);
	UWidgetBlueprint* CardBlueprint = Load<UWidgetBlueprint>(CardWBPPath, Log);
	UTexture2D* KeyEscTexture = Load<UTexture2D>(KeyEscPath, Log);
	UTexture2D* KeyEnterTexture = Load<UTexture2D>(KeyEnterPath, Log);
	UObject* Hakgyoansim = Load<UObject>(HakgyoansimFontPath, Log);
	if (!Blueprint || !Blueprint->WidgetTree) return false;

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();

	// 1920x1080 기준 캔버스 — 해상도가 바뀌면 DPI 스케일이 맞춘다
	UCanvasPanel* Root = ResetTree<UCanvasPanel>(Blueprint, TEXT("CanvasPanel_Root"));
	const FAnchors Stretch(0.0f, 0.0f, 1.0f, 1.0f);
	const FAnchors TopCenter(0.5f, 0.0f);
	const FAnchors BottomRight(1.0f, 1.0f);

	// 1) 로비 화면을 살짝 흐리고 어둡게 덮는다 (시안의 배경은 로비 복도 그림이라 따로 받지 않았다)
	UBackgroundBlur* Blur = Make<UBackgroundBlur>(Tree, TEXT("Blur_Background"));
	Blur->SetBlurStrength(Spec::BlurStrength);
	Blur->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToCanvas(Root, Blur, Stretch, FVector2D::ZeroVector, FVector2D::ZeroVector, false);

	UImage* Dim = Make<UImage>(Tree, TEXT("Image_Dim"));
	Dim->SetBrush(MakeColorBrush(Spec::DimColor));
	// ★Visible★ — 화면 뒤의 게임 월드로 클릭이 새지 않게 막는다
	Dim->SetVisibility(ESlateVisibility::Visible);
	AddToCanvas(Root, Dim, Stretch, FVector2D::ZeroVector, FVector2D::ZeroVector, false);

	// 2) 제목 — 가운데, 학교안심 55px
	UTextBlock* Title = MakeText(Tree, TEXT("Txt_ScreenTitle"), NSLOCTEXT("DRStageSelect", "ScreenTitle", "스테이지 선택"), Hakgyoansim, Spec::ScreenTitlePx, false);
	Title->SetJustification(ETextJustify::Center);
	AddToCanvas(Root, Title, TopCenter, FVector2D(0.5, 0.0), FVector2D(0.0, Spec::ScreenTitleTop), true);

	// 3) 카드 줄 — ★이름 Box_Cards 고정★ C++ 이 카드를 아래쪽 맞춤 · 23px 간격으로 채운다
	UHorizontalBox* Cards = Make<UHorizontalBox>(Tree, TEXT("Box_Cards"), true);
	AddToCanvas(Root, Cards, TopCenter, FVector2D(0.5, 0.0), FVector2D(0.0, Spec::CardsTop), true);

	// 4) 키 안내 — [ESC] BACK   [ENTER] START (클릭도 된다)
	UHorizontalBox* KeyHints = Make<UHorizontalBox>(Tree, TEXT("HorizontalBox_KeyHints"));
	AddToCanvas(Root, KeyHints, BottomRight, FVector2D(1.0, 0.5), Spec::KeyHintsAnchorOffset, true);
	{
		struct FKeySpec
		{
			const TCHAR* ButtonName;
			const TCHAR* RowName;
			const TCHAR* IconName;
			const TCHAR* LabelName;
			UTexture2D* Icon;
			FText Label;
			float LabelPx;
			float LabelGap;
			float ButtonGap;
		};
		const FKeySpec Keys[] = {
			{ TEXT("Btn_Back"), TEXT("HorizontalBox_Back"), TEXT("Image_KeyEsc"), TEXT("Txt_Back"), KeyEscTexture,
				NSLOCTEXT("DRStageSelect", "KeyHintBack", "BACK"), Spec::BackPx, Spec::KeyLabelGapBack, 0.0f },
			{ TEXT("Btn_Start"), TEXT("HorizontalBox_Start"), TEXT("Image_KeyEnter"), TEXT("Txt_Start"), KeyEnterTexture,
				NSLOCTEXT("DRStageSelect", "KeyHintStart", "START"), Spec::StartPx, Spec::KeyLabelGapStart, Spec::KeyButtonGap },
		};

		for (const FKeySpec& Key : Keys)
		{
			UButton* Button = Make<UButton>(Tree, Key.ButtonName, true);
			StyleKeyHintButton(Button);
			AddToHBox(KeyHints, Button, VAlign_Center, FMargin(Key.ButtonGap, 0.0f, 0.0f, 0.0f));

			UHorizontalBox* Row = Make<UHorizontalBox>(Tree, Key.RowName);
			Row->SetVisibility(ESlateVisibility::HitTestInvisible);
			// 버튼 슬롯 기본 여백(4,2)을 없앤다 — 안쪽 여백은 스타일의 KeyButtonPadding 하나로 (남겨 두면 BACK~ENTER 가 8px 벌어진다)
			if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Button->SetContent(Row)))
			{
				ContentSlot->SetPadding(FMargin(0.0f));
			}

			AddToHBox(Row, MakeImage(Tree, Key.IconName, Key.Icon, false, SizeForHeight(Key.Icon, Spec::KeyIconHeight)), VAlign_Center);
			AddToHBox(Row, MakeText(Tree, Key.LabelName, Key.Label, Hakgyoansim, Key.LabelPx, false), VAlign_Center,
				FMargin(Key.LabelGap, 0.0f, 0.0f, 0.0f));
		}
	}

	if (!CompileAndMark(Blueprint, Log)) return false;

	if (CardBlueprint && CardBlueprint->GeneratedClass)
	{
		SetObjectProperty(Blueprint->GeneratedClass->GetDefaultObject(), TEXT("CardWidgetClass"), CardBlueprint->GeneratedClass.Get(), Log);
	}
	return CompileAndMark(Blueprint, Log);	// CDO 를 고쳤으면 다시 컴파일 (옷장 T-32)
#else
	return false;
#endif
}

bool UDRStageSelectEditorTools::SetWidgetBlueprintText(const FString& WidgetBlueprintPath, FName WidgetName, const FString& NewText, FString& OutLog)
{
#if WITH_EDITOR
	using namespace DRStageSelectEditor;
	OutLog.Reset();
	FLog Log{ OutLog };

	UWidgetBlueprint* Blueprint = Load<UWidgetBlueprint>(*WidgetBlueprintPath, Log);
	if (!Blueprint || !Blueprint->WidgetTree) return false;

	UTextBlock* TextBlock = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(WidgetName));
	if (!TextBlock)
	{
		Log.Add(FString::Printf(TEXT("TextBlock 없음: %s"), *WidgetName.ToString()));
		return false;
	}

	Blueprint->Modify();
	TextBlock->Modify();

	// 기존 글자의 네임스페이스/키를 그대로 쓴다 — 번역 테이블과의 연결이 유지된다
	const FText OldText = TextBlock->GetText();
	const TOptional<FString> Namespace = FTextInspector::GetNamespace(OldText);
	const TOptional<FString> Key = FTextInspector::GetKey(OldText);
	const FText Updated = (Namespace.IsSet() && Key.IsSet() && !Key.GetValue().IsEmpty())
		? FText::ChangeKey(FTextKey(Namespace.GetValue()), FTextKey(Key.GetValue()), FText::FromString(NewText))
		: FText::FromString(NewText);
	TextBlock->SetText(Updated);
	Log.Add(FString::Printf(TEXT("%s.%s: \"%s\" → \"%s\" (키 %s)"), *Blueprint->GetName(), *WidgetName.ToString(),
		*OldText.ToString(), *NewText, Key.IsSet() ? *Key.GetValue() : TEXT("새로")));

	return CompileAndMark(Blueprint, Log);
#else
	return false;
#endif
}
