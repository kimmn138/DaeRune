// Copyright DaeRune


#include "DRStageSelectEditorTools.h"

#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Fonts/CompositeFont.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
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
	const TCHAR* BackgroundMaterialPath = TEXT("/Game/Blueprints/UI/StageSelect/Material/M_UI_StageSelect_BG.M_UI_StageSelect_BG");
	const TCHAR* ThumbnailMaterialPath = TEXT("/Game/Blueprints/UI/StageSelect/Material/M_UI_StageSelect_Thumb.M_UI_StageSelect_Thumb");
	// 시안 폰트는 Pretendard — 프로젝트에 있는 굵기는 SemiBold 하나다 (시안의 700/800 굵기는 표현 못 한다)
	const TCHAR* BodyFontPath = TEXT("/Game/DaeRuneAssets/UI/Font/Pretendard-SemiBold__1__Font.Pretendard-SemiBold__1__Font");

	struct FLog
	{
		FString& Out;
		void Add(const FString& Line)
		{
			Out += Line + TEXT("\n");
			UE_LOG(LogDR, Display, TEXT("[StageSelectEditor] %s"), *Line);
		}
	};



	// ===== 시안 :root 변수 =====
	const FLinearColor Line = SRGB(0x8F, 0xE3, 0xFF);	// --line
	const FLinearColor Line2 = SRGB(0x4F, 0xB4, 0xE6);	// --line2
	const FLinearColor Text = SRGB(0xE6, 0xF4, 0xFF);	// --text
	const FLinearColor Sub = SRGB(0x9D, 0xB9, 0xD6);		// --sub
	const FLinearColor Ink = SRGB(0x04, 0x12, 0x2A);		// 시작 버튼 글자


	// Px 는 시안의 CSS px. UMG 폰트 크기는 72dpi 포인트라 ×0.75. LetterSpacingEm 은 CSS letter-spacing(em)
	FSlateFontInfo MakeFont(const UObject* FontObject, float Px, float LetterSpacingEm = 0.0f)
	{
		FSlateFontInfo Font(FontObject, Px * 0.75f, PickTypeface(FontObject));
		Font.LetterSpacing = FMath::RoundToInt(LetterSpacingEm * 1000.0f);
		return Font;
	}

	FSlateBrush MakeColorBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

	// 둥근 사각형. Fill 의 알파가 0 이면 외곽선만 그린다
	FSlateBrush MakeRoundedBrush(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.0f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), OutlineWidth);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	FSlateBrush MakeMaterialBrush(UObject* Material)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetResourceObject(Material);
		Brush.ImageSize = FVector2D(32.0, 32.0);
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

	UOverlaySlot* AddToOverlay(UOverlay* Parent, UWidget* Child, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Padding = FMargin(0.0f))
	{
		UOverlaySlot* OverlaySlot = Parent->AddChildToOverlay(Child);
		OverlaySlot->SetHorizontalAlignment(H);
		OverlaySlot->SetVerticalAlignment(V);
		OverlaySlot->SetPadding(Padding);
		return OverlaySlot;
	}

	UVerticalBoxSlot* AddToVBox(UVerticalBox* Parent, UWidget* Child, bool bFill, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Padding = FMargin(0.0f))
	{
		UVerticalBoxSlot* BoxSlot = Parent->AddChildToVerticalBox(Child);
		BoxSlot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		BoxSlot->SetHorizontalAlignment(H);
		BoxSlot->SetVerticalAlignment(V);
		BoxSlot->SetPadding(Padding);
		return BoxSlot;
	}

	UHorizontalBoxSlot* AddToHBox(UHorizontalBox* Parent, UWidget* Child, bool bFill, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Padding = FMargin(0.0f))
	{
		UHorizontalBoxSlot* BoxSlot = Parent->AddChildToHorizontalBox(Child);
		BoxSlot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		BoxSlot->SetHorizontalAlignment(H);
		BoxSlot->SetVerticalAlignment(V);
		BoxSlot->SetPadding(Padding);
		return BoxSlot;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const TCHAR* Name, const FString& Sample, const UObject* Font, float Px,
		const FLinearColor& Color, bool bVariable, float LetterSpacingEm = 0.0f)
	{
		UTextBlock* TextBlock = Make<UTextBlock>(Tree, Name, bVariable);
		// 런타임 글자는 C++ 이 넣는다. 고정 문구가 아닌 것은 번역 수집에서 빠지도록 CultureInvariant 로 둔다
		TextBlock->SetText(FText::AsCultureInvariant(Sample));
		TextBlock->SetFont(MakeFont(Font, Px, LetterSpacingEm));
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		return TextBlock;
	}

	// 버튼 4상태. 배경은 둥근 사각형 + 외곽선 (시안 .btn)
	void StyleButton(UButton* Button, const FLinearColor& Fill, const FLinearColor& Outline,
		const FLinearColor& HoverFill, const FLinearColor& HoverOutline)
	{
		FButtonStyle Style = Button->GetStyle();
		const FSlateBrush Normal = MakeRoundedBrush(Fill, 4.0f, Outline, 3.0f);
		const FSlateBrush Hovered = MakeRoundedBrush(HoverFill, 4.0f, HoverOutline, 3.0f);
		FSlateBrush Pressed = Hovered;
		Pressed.TintColor = FSlateColor(HoverFill * FLinearColor(0.85f, 0.85f, 0.85f, 1.0f));
		// 시안 .btn:disabled{opacity:.35}
		const FSlateBrush Disabled = MakeRoundedBrush(Fill * FLinearColor(1.0f, 1.0f, 1.0f, 0.35f), 4.0f,
			Outline * FLinearColor(1.0f, 1.0f, 1.0f, 0.35f), 3.0f);
		Style.SetNormal(Normal).SetHovered(Hovered).SetPressed(Pressed).SetDisabled(Disabled);
		Style.SetNormalPadding(FMargin(48.0f, 0.0f)).SetPressedPadding(FMargin(48.0f, 2.0f, 48.0f, 0.0f));
		Button->SetStyle(Style);

		// 버튼이 키보드 포커스를 가져가지 않게 — 키 입력은 화면이 받는다 (Enter / ESC / ←→)
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
	UMaterialInterface* ThumbnailMaterial = Load<UMaterialInterface>(ThumbnailMaterialPath, Log);
	UObject* Font = Load<UObject>(BodyFontPath, Log);
	if (!Blueprint || !Blueprint->WidgetTree) return false;

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();

	// 카드 = Overlay. 뒤에 넣은 것이 위에 그려진다 — 패널 바탕 → 내용 → 선택 표시 → 테두리
	UOverlay* Root = ResetTree<UOverlay>(Blueprint, TEXT("Overlay_Card"));

	// 1) 카드 바탕 (시안 --panel rgba(150,200,240,.10), radius 6)
	UBorder* PanelBack = Make<UBorder>(Tree, TEXT("Border_Panel"));
	PanelBack->SetBrush(MakeRoundedBrush(SRGB(150, 200, 240, 0.10f), 6.0f));
	PanelBack->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToOverlay(Root, PanelBack, HAlign_Fill, VAlign_Fill);

	// 2) 내용 — 테두리 두께(2)만큼 안쪽
	UVerticalBox* Body = Make<UVerticalBox>(Tree, TEXT("VerticalBox_Body"));
	Body->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToOverlay(Root, Body, HAlign_Fill, VAlign_Fill, FMargin(2.0f));

	// 2-1) 썸네일 영역 (flex:1)
	UOverlay* Thumb = Make<UOverlay>(Tree, TEXT("Overlay_Thumb"));
	AddToVBox(Body, Thumb, true, HAlign_Fill, VAlign_Fill);

	// 배경 이미지 — 아트가 없으면 그라데이션 머티리얼(시안 .thumb 기본 배경). C++ 이 잠긴 카드는 어둡게 틴트한다
	UImage* Thumbnail = Make<UImage>(Tree, TEXT("Image_Thumbnail"), true);
	Thumbnail->SetBrush(ThumbnailMaterial ? MakeMaterialBrush(ThumbnailMaterial) : MakeColorBrush(SRGB(0x2A, 0x4A, 0x7C)));
	Thumbnail->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToOverlay(Thumb, Thumbnail, HAlign_Fill, VAlign_Fill);

	// 큰 번호 — 130px, rgba(230,244,255,.9), 글로우 대신 그림자 (left 32, top 10)
	UTextBlock* Number = MakeText(Tree, TEXT("Txt_Number"), TEXT("01"), Font, 130.0f, SRGB(230, 244, 255, 0.9f), true);
	Number->SetShadowOffset(FVector2D(0.0, 2.0));
	Number->SetShadowColorAndOpacity(SRGB(5, 15, 35, 0.75f));
	AddToOverlay(Thumb, Number, HAlign_Left, VAlign_Top, FMargin(32.0f, 10.0f, 0.0f, 0.0f));

	// 배지 — right 48, top 24, padding 8 22, 테두리 1.5, 배경 rgba(5,15,35,.7). 외곽선 색은 C++ 이 상태별로 바꾼다
	UBorder* Badge = Make<UBorder>(Tree, TEXT("Border_Badge"), true);
	Badge->SetBrush(MakeRoundedBrush(SRGB(5, 15, 35, 0.7f), 3.0f, Line, 1.5f));
	Badge->SetPadding(FMargin(22.0f, 8.0f));
	Badge->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToOverlay(Thumb, Badge, HAlign_Right, VAlign_Top, FMargin(0.0f, 24.0f, 48.0f, 0.0f));
	Badge->SetContent(MakeText(Tree, TEXT("Txt_Badge"), TEXT("진행 가능"), Font, 20.0f, Line, true, 0.14f));

	// CLEARED 도장 — 가운데, -12도, 78px, 테두리 8 (시안은 double 테두리 — UMG 는 한 줄만)
	UBorder* Stamp = Make<UBorder>(Tree, TEXT("Panel_Cleared"), true);
	Stamp->SetBrush(MakeRoundedBrush(SRGB(4, 18, 30, 0.55f), 8.0f, Line, 8.0f));
	Stamp->SetPadding(FMargin(44.0f, 10.0f));
	Stamp->SetRenderTransformAngle(-12.0f);
	Stamp->SetVisibility(ESlateVisibility::Collapsed);	// 클리어일 때 C++ 이 켠다
	AddToOverlay(Thumb, Stamp, HAlign_Center, VAlign_Center);
	Stamp->SetContent(MakeText(Tree, TEXT("Txt_Stamp"), TEXT("CLEARED"), Font, 78.0f, Line, false, 0.16f));

	// 잠김 덮개 — 어두운 막 rgba(5,10,25,.55) + 자물쇠 (이모지 글리프가 폰트에 없어 도형으로 그린다)
	UOverlay* Locked = Make<UOverlay>(Tree, TEXT("Panel_Locked"), true);
	Locked->SetVisibility(ESlateVisibility::Collapsed);	// 잠겼을 때 C++ 이 켠다
	AddToOverlay(Thumb, Locked, HAlign_Fill, VAlign_Fill);
	{
		UImage* Dim = Make<UImage>(Tree, TEXT("Image_LockDim"));
		Dim->SetBrush(MakeColorBrush(SRGB(5, 10, 25, 0.55f)));
		AddToOverlay(Locked, Dim, HAlign_Fill, VAlign_Fill);

		const FLinearColor LockColor = SRGB(0xC9, 0xD6, 0xE6);
		UVerticalBox* LockIcon = Make<UVerticalBox>(Tree, TEXT("VerticalBox_LockIcon"));
		AddToOverlay(Locked, LockIcon, HAlign_Center, VAlign_Center);

		// 고리: 외곽선만 있는 둥근 사각형의 위쪽 절반 (아래 절반은 몸통이 덮는다)
		USizeBox* ShackleBox = Make<USizeBox>(Tree, TEXT("SizeBox_LockShackle"));
		ShackleBox->SetWidthOverride(64.0f);
		ShackleBox->SetHeightOverride(70.0f);
		UBorder* Shackle = Make<UBorder>(Tree, TEXT("Border_LockShackle"));
		Shackle->SetBrush(MakeRoundedBrush(FLinearColor::Transparent, 32.0f, LockColor, 11.0f));
		ShackleBox->SetContent(Shackle);
		AddToVBox(LockIcon, ShackleBox, false, HAlign_Center, VAlign_Top, FMargin(0.0f, 0.0f, 0.0f, -34.0f));

		// 몸통
		USizeBox* BodyBox = Make<USizeBox>(Tree, TEXT("SizeBox_LockBody"));
		BodyBox->SetWidthOverride(104.0f);
		BodyBox->SetHeightOverride(82.0f);
		UBorder* LockBody = Make<UBorder>(Tree, TEXT("Border_LockBody"));
		LockBody->SetBrush(MakeRoundedBrush(LockColor, 12.0f));
		BodyBox->SetContent(LockBody);
		AddToVBox(LockIcon, BodyBox, false, HAlign_Center, VAlign_Top);
	}

	// 2-2) 정보 영역 — padding 28 36 34, 배경 rgba(5,15,35,.55)
	UBorder* Info = Make<UBorder>(Tree, TEXT("Border_Info"));
	Info->SetBrush(MakeColorBrush(SRGB(5, 15, 35, 0.55f)));
	Info->SetPadding(FMargin(36.0f, 28.0f, 36.0f, 34.0f));
	Info->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToVBox(Body, Info, false, HAlign_Fill, VAlign_Fill);
	{
		UVerticalBox* InfoBox = Make<UVerticalBox>(Tree, TEXT("VerticalBox_Info"));
		Info->SetContent(InfoBox);

		AddToVBox(InfoBox, MakeText(Tree, TEXT("Txt_Title"), TEXT("스테이지 1 · 연구 격납고"), Font, 36.0f, Text, true),
			false, HAlign_Left, VAlign_Top, FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		UTextBlock* Mission = MakeText(Tree, TEXT("Txt_Mission"), TEXT("적들로부터 클렌저를 지키세요"), Font, 24.0f, Sub, true);
		Mission->SetAutoWrapText(true);
		AddToVBox(InfoBox, Mission, false, HAlign_Fill, VAlign_Top);

		UTextBlock* Done = MakeText(Tree, TEXT("Txt_Done"), TEXT("임무 완료 · 다시 도전 가능"), Font, 22.0f, Line, true);
		Done->SetVisibility(ESlateVisibility::Collapsed);	// 클리어일 때 C++ 이 켠다
		AddToVBox(InfoBox, Done, false, HAlign_Left, VAlign_Top, FMargin(0.0f, 12.0f, 0.0f, 0.0f));
	}

	// 3) 선택 표시 — 시안 .card.sel 의 밝은 바탕 (rgba(143,227,255,.18) - 기본 .10 ≈ +.08)
	UBorder* Selected = Make<UBorder>(Tree, TEXT("Panel_Selected"), true);
	Selected->SetBrush(MakeRoundedBrush(SRGB(143, 227, 255, 0.08f), 6.0f));
	Selected->SetVisibility(ESlateVisibility::Collapsed);	// 선택됐을 때 C++ 이 켠다
	AddToOverlay(Root, Selected, HAlign_Fill, VAlign_Fill);

	// 4) 테두리 — 외곽선만. 실제 색은 런타임에 C++ 이 외곽선 색을 바꾼다 (기본 / 강조 / 잠김 / 경고)
	UBorder* Frame = Make<UBorder>(Tree, TEXT("Border_Frame"), true);
	Frame->SetBrush(MakeRoundedBrush(FLinearColor::Transparent, 6.0f, Line2, 2.0f));
	Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddToOverlay(Root, Frame, HAlign_Fill, VAlign_Fill);

	return CompileAndMark(Blueprint, Log);
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
	UMaterialInterface* BackgroundMaterial = Load<UMaterialInterface>(BackgroundMaterialPath, Log);
	UObject* Font = Load<UObject>(BodyFontPath, Log);
	if (!Blueprint || !Blueprint->WidgetTree) return false;

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();

	// 1920x1080 기준 (시안 .wrap) — Overlay 루트라 해상도가 바뀌면 DPI 스케일이 맞춘다
	UOverlay* Root = ResetTree<UOverlay>(Blueprint, TEXT("Overlay_Root"));

	// 1) 배경 — 슬레이트 블루 그라데이션 + 좌상단 광원 + 왼쪽 가로 줄무늬 (머티리얼 하나로)
	UImage* Background = Make<UImage>(Tree, TEXT("Image_Background"));
	Background->SetBrush(BackgroundMaterial ? MakeMaterialBrush(BackgroundMaterial) : MakeColorBrush(SRGB(0x41, 0x52, 0x69)));
	// ★Visible★ — 화면 뒤의 게임 월드로 클릭이 새지 않게 막는다
	Background->SetVisibility(ESlateVisibility::Visible);
	AddToOverlay(Root, Background, HAlign_Fill, VAlign_Fill);

	// 2) 내용 — padding 56 120 48
	UVerticalBox* Content = Make<UVerticalBox>(Tree, TEXT("VerticalBox_Content"));
	AddToOverlay(Root, Content, HAlign_Fill, VAlign_Fill, FMargin(120.0f, 56.0f, 120.0f, 48.0f));

	// 2-1) 머리말 — 제목(좌) · 클리어 수(우), 아래 정렬
	UHorizontalBox* Header = Make<UHorizontalBox>(Tree, TEXT("HorizontalBox_Header"));
	AddToVBox(Content, Header, false, HAlign_Fill, VAlign_Top, FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	{
		UVerticalBox* TitleBox = Make<UVerticalBox>(Tree, TEXT("VerticalBox_Title"));
		AddToHBox(Header, TitleBox, true, HAlign_Left, VAlign_Bottom);

		UTextBlock* SubTitle = MakeText(Tree, TEXT("Txt_SubTitle"), TEXT(""), Font, 18.0f, Sub, false, 0.3f);
		SubTitle->SetText(NSLOCTEXT("DRStageSelect", "ScreenSubTitle", "STAGE SELECT"));
		AddToVBox(TitleBox, SubTitle, false, HAlign_Left, VAlign_Top, FMargin(0.0f, 0.0f, 0.0f, 4.0f));

		UTextBlock* Title = MakeText(Tree, TEXT("Txt_ScreenTitle"), TEXT(""), Font, 48.0f, Text, false, 0.08f);
		Title->SetText(NSLOCTEXT("DRStageSelect", "ScreenTitle", "스테이지 선택"));
		AddToVBox(TitleBox, Title, false, HAlign_Left, VAlign_Top);

		// "클리어 0 / 2" — 숫자만 크게 (시안 .prog b)
		UHorizontalBox* Progress = Make<UHorizontalBox>(Tree, TEXT("HorizontalBox_Progress"));
		AddToHBox(Header, Progress, false, HAlign_Right, VAlign_Bottom);

		UTextBlock* ClearLabel = MakeText(Tree, TEXT("Txt_ClearLabel"), TEXT(""), Font, 22.0f, Sub, false);
		ClearLabel->SetText(NSLOCTEXT("DRStageSelect", "ClearLabel", "클리어"));
		AddToHBox(Progress, ClearLabel, false, HAlign_Left, VAlign_Bottom, FMargin(0.0f, 0.0f, 8.0f, 4.0f));
		AddToHBox(Progress, MakeText(Tree, TEXT("Txt_ClearCount"), TEXT("0"), Font, 34.0f, Line, true),
			false, HAlign_Left, VAlign_Bottom);
		AddToHBox(Progress, MakeText(Tree, TEXT("Txt_ClearSlash"), TEXT("/"), Font, 22.0f, Sub, false),
			false, HAlign_Left, VAlign_Bottom, FMargin(8.0f, 0.0f, 8.0f, 4.0f));
		AddToHBox(Progress, MakeText(Tree, TEXT("Txt_StageCount"), TEXT("2"), Font, 22.0f, Sub, true),
			false, HAlign_Left, VAlign_Bottom, FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	// 머리말 밑줄 2px --line2
	{
		USizeBox* LineBox = Make<USizeBox>(Tree, TEXT("SizeBox_HeaderLine"));
		LineBox->SetHeightOverride(2.0f);
		UImage* HeaderLine = Make<UImage>(Tree, TEXT("Image_HeaderLine"));
		HeaderLine->SetBrush(MakeColorBrush(Line2));
		LineBox->SetContent(HeaderLine);
		AddToVBox(Content, LineBox, false, HAlign_Fill, VAlign_Top);
	}

	// 2-2) 카드 영역 — 위아래 gap 36, 높이 680 (시안 .card max-height), 가운데
	{
		USizeBox* CardsBox = Make<USizeBox>(Tree, TEXT("SizeBox_Cards"));
		CardsBox->SetHeightOverride(680.0f);
		AddToVBox(Content, CardsBox, true, HAlign_Fill, VAlign_Center, FMargin(0.0f, 36.0f));

		// ★이름 Box_Cards 고정★ — C++ 이 카드를 같은 폭으로 채운다 (CardSpacing = 64)
		UHorizontalBox* Cards = Make<UHorizontalBox>(Tree, TEXT("Box_Cards"), true);
		CardsBox->SetContent(Cards);
	}

	// 바닥글 윗줄 2px --line2
	{
		USizeBox* LineBox = Make<USizeBox>(Tree, TEXT("SizeBox_FooterLine"));
		LineBox->SetHeightOverride(2.0f);
		UImage* FooterLine = Make<UImage>(Tree, TEXT("Image_FooterLine"));
		FooterLine->SetBrush(MakeColorBrush(Line2));
		LineBox->SetContent(FooterLine);
		AddToVBox(Content, LineBox, false, HAlign_Fill, VAlign_Top);
	}

	// 2-3) 바닥글 — 문구(남는 폭) · 뒤로 · 시작, gap 32, padding-top 14
	UHorizontalBox* Footer = Make<UHorizontalBox>(Tree, TEXT("HorizontalBox_Footer"));
	AddToVBox(Content, Footer, false, HAlign_Fill, VAlign_Top, FMargin(0.0f, 14.0f, 0.0f, 0.0f));
	{
		UTextBlock* Message = MakeText(Tree, TEXT("Txt_Message"), TEXT("스테이지를 선택하세요."), Font, 24.0f, Sub, true);
		Message->SetAutoWrapText(true);
		AddToHBox(Footer, Message, true, HAlign_Fill, VAlign_Center);

		struct FButtonSpec
		{
			const TCHAR* Name;
			const TCHAR* SizeName;
			const TCHAR* LabelName;
			FText Label;
			bool bPrimary;
		};
		const FButtonSpec Specs[] = {
			{ TEXT("Btn_Back"), TEXT("SizeBox_Back"), TEXT("Txt_Back"), NSLOCTEXT("DRStageSelect", "BackButton", "뒤로 (ESC)"), false },
			{ TEXT("Btn_Start"), TEXT("SizeBox_Start"), TEXT("Txt_Start"), NSLOCTEXT("DRStageSelect", "StartButton", "시작 (Enter)"), true },
		};

		for (const FButtonSpec& Spec : Specs)
		{
			// min-width 260, height 84
			USizeBox* SizeBox = Make<USizeBox>(Tree, Spec.SizeName);
			SizeBox->SetMinDesiredWidth(260.0f);
			SizeBox->SetHeightOverride(84.0f);
			AddToHBox(Footer, SizeBox, false, HAlign_Right, VAlign_Center, FMargin(32.0f, 0.0f, 0.0f, 0.0f));

			UButton* Button = Make<UButton>(Tree, Spec.Name, true);
			if (Spec.bPrimary)
			{
				// 시안 .btn.go — 그라데이션(#2a7fc0→#4fb4e6) 대신 중간색, 글자 진남색
				StyleButton(Button, SRGB(0x3C, 0x9A, 0xD3), Line, SRGB(0x5C, 0xB9, 0xEA), Line);
			}
			else
			{
				StyleButton(Button, SRGB(150, 200, 240, 0.10f), Line2, SRGB(143, 227, 255, 0.20f), Line);
			}
			SizeBox->SetContent(Button);

			UTextBlock* Label = MakeText(Tree, Spec.LabelName, TEXT(""), Font, 28.0f, Spec.bPrimary ? Ink : Text, false, 0.1f);
			Label->SetText(Spec.Label);
			Label->SetJustification(ETextJustify::Center);
			Button->SetContent(Label);
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
