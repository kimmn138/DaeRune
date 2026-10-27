// Copyright DaeRune


#include "Editor/DRWardrobeEditorTools.h"

#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/Font.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInterface.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UI/Widget/DRWardrobeTabWidget.h"
#include "DaeRune/DRLogChannels.h"
#include "DREditorToolCommon.h"
#endif

#if WITH_EDITOR
namespace DRWardrobeEditor
{
	// 세 도구 공용 헬퍼 (DREditorToolCommon.h)
	using DREditorToolCommon::SRGB;
	using DREditorToolCommon::PickTypeface;
	using DREditorToolCommon::DiscardRecursive;
	using DREditorToolCommon::Load;
	using DREditorToolCommon::CompileAndMark;
	using DREditorToolCommon::SetObjectProperty;

	// ===== 에셋 경로 (Plan(옷장) D-27) =====
	const TCHAR* TabWBPPath = TEXT("/Game/Blueprints/UI/Closet/WBP_WardrobeTab.WBP_WardrobeTab");
	const TCHAR* SlotWBPPath = TEXT("/Game/Blueprints/UI/Closet/WBP_WardrobeSlot.WBP_WardrobeSlot");
	const TCHAR* ScreenWBPPath = TEXT("/Game/Blueprints/UI/Closet/WBP_Wardrobe.WBP_Wardrobe");
	const TCHAR* PreviewWBPPath = TEXT("/Game/Blueprints/UI/Closet/WBP_CosmeticPreview.WBP_CosmeticPreview");

	const TCHAR* PillTexPath = TEXT("/Game/DaeRuneAssets/UI/Closet/카테고리_선택박스.카테고리_선택박스");
	const TCHAR* TabBarTexPath = TEXT("/Game/DaeRuneAssets/UI/Closet/카테고리바.카테고리바");
	const TCHAR* PanelTexPath = TEXT("/Game/DaeRuneAssets/UI/Closet/옷장.옷장");
	const TCHAR* SlotNormalTexPath = TEXT("/Game/DaeRuneAssets/UI/Closet/Slot/속성_1_기본.속성_1_기본");
	const TCHAR* SlotHoveredTexPath = TEXT("/Game/DaeRuneAssets/UI/Closet/Slot/속성_1_호버.속성_1_호버");
	const TCHAR* SlotSelectedTexPath = TEXT("/Game/DaeRuneAssets/UI/Closet/Slot/속성_1_선택.속성_1_선택");
	const TCHAR* ClearAllTexPath = TEXT("/Game/DaeRuneAssets/UI/Settimgs/clear_all.clear_all");

	const TCHAR* GlowMIPath = TEXT("/Game/Blueprints/UI/Closet/Material/MI_UI_Wardrobe_BGGlow.MI_UI_Wardrobe_BGGlow");
	const TCHAR* CharBackMIPath = TEXT("/Game/Blueprints/UI/Closet/Material/MI_UI_Wardrobe_CharBack.MI_UI_Wardrobe_CharBack");
	const TCHAR* ScreenFXMIPath = TEXT("/Game/Blueprints/UI/Closet/Material/MI_UI_Wardrobe_ScreenFX.MI_UI_Wardrobe_ScreenFX");
	const TCHAR* PreviewMaterialPath = TEXT("/Game/Blueprints/UI/Closet/Material/M_CosmeticPreview.M_CosmeticPreview");

	const TCHAR* TitleFontPath = TEXT("/Game/DaeRuneAssets/UI/Font/HakgyoansimUndongjangL_Font.HakgyoansimUndongjangL_Font");
	const TCHAR* LabelFontPath = TEXT("/Game/DaeRuneAssets/UI/Font/BrunoAceSC-Regular_Font.BrunoAceSC-Regular_Font");
	const TCHAR* BodyFontPath = TEXT("/Game/DaeRuneAssets/UI/Font/Pretendard-SemiBold__1__Font.Pretendard-SemiBold__1__Font");

	struct FLog
	{
		FString& Out;
		void Add(const FString& Line)
		{
			Out += Line + TEXT("\n");
			UE_LOG(LogDR, Display, TEXT("[WardrobeEditor] %s"), *Line);
		}
	};




	FSlateFontInfo MakeFont(const UObject* FontObject, float Size)
	{
		return FSlateFontInfo(FontObject, Size, PickTypeface(FontObject));
	}

	FSlateBrush MakeImageBrush(UObject* Resource, const FVector2D& Size)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Resource);
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		return Brush;
	}

	// 배경은 아래 이미지가 그린다 — 버튼은 히트박스와 이벤트만
	void MakeTransparentButton(UButton* Button)
	{
		FButtonStyle Style = Button->GetStyle();
		FSlateBrush NoDraw;
		NoDraw.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.SetNormal(NoDraw).SetHovered(NoDraw).SetPressed(NoDraw).SetDisabled(NoDraw);
		Style.SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f));
		Button->SetStyle(Style);
	}

	template <typename T>
	T* Find(UWidgetTree* Tree, const TCHAR* Name, FLog& Log)
	{
		T* Widget = Cast<T>(Tree->FindWidget(FName(Name)));
		if (!Widget)
		{
			Log.Add(FString::Printf(TEXT("위젯 없음: %s"), Name));
		}
		return Widget;
	}


	UCanvasPanelSlot* PlaceOnCanvas(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment,
		const FVector2D& Position, const FVector2D& Size, bool bAutoSize = false)
	{
		UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
		if (!CanvasSlot) return nullptr;

		CanvasSlot->SetAnchors(Anchors);
		CanvasSlot->SetAlignment(Alignment);
		CanvasSlot->SetAutoSize(bAutoSize);
		// 앵커가 한 점이면 (X, Y, 폭, 높이), 늘이기면 (왼, 위, 오른, 아래 여백)
		CanvasSlot->SetOffsets(FMargin(Position.X, Position.Y, Size.X, Size.Y));
		return CanvasSlot;
	}

	void SetOverlaySlot(UWidget* Widget, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Padding = FMargin(0.0f))
	{
		if (UOverlaySlot* OverlaySlot = Widget ? Cast<UOverlaySlot>(Widget->Slot) : nullptr)
		{
			OverlaySlot->SetHorizontalAlignment(H);
			OverlaySlot->SetVerticalAlignment(V);
			OverlaySlot->SetPadding(Padding);
		}
	}

	void RemoveFunctionGraph(UBlueprint* Blueprint, FName GraphName, FLog& Log)
	{
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetFName() == GraphName)
			{
				FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph, EGraphRemoveFlags::Recompile);
				Log.Add(FString::Printf(TEXT("BP 함수 제거: %s (C++ 로 옮김)"), *GraphName.ToString()));
				return;
			}
		}
	}

	void RemoveVariable(UBlueprint* Blueprint, FName VarName, FLog& Log)
	{
		if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName) != INDEX_NONE)
		{
			FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VarName);
			Log.Add(FString::Printf(TEXT("BP 변수 제거: %s (C++ 변수로 대체)"), *VarName.ToString()));
		}
	}



	UObject* GetCDO(UWidgetBlueprint* Blueprint)
	{
		return Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
	}
}
#endif

bool UDRWardrobeEditorTools::ImportPropertyText(UObject* Target, FName PropertyName, const FString& Text, FString& OutError)
{
#if WITH_EDITOR
	OutError.Reset();

	if (!Target)
	{
		OutError = TEXT("Target 이 null 입니다.");
		return false;
	}

	FProperty* Property = FindFProperty<FProperty>(Target->GetClass(), PropertyName);
	if (!Property)
	{
		OutError = FString::Printf(TEXT("%s 에 '%s' 프로퍼티가 없습니다."), *Target->GetClass()->GetName(), *PropertyName.ToString());
		return false;
	}

	Target->Modify();

	FStringOutputDevice Errors;
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Target);
	const TCHAR* End = Property->ImportText_Direct(*Text, ValuePtr, Target, PPF_None, &Errors);
	if (!Errors.IsEmpty())
	{
		OutError = Errors;
	}
	if (!End)
	{
		if (OutError.IsEmpty()) OutError = TEXT("ImportText 실패 (형식 오류)");
		return false;
	}

	// 손으로 고친 것과 같게 — 카탈로그 검증 등 PostEditChangeProperty 훅이 돈다
	FPropertyChangedEvent ChangedEvent(Property, EPropertyChangeType::ValueSet);
	Target->PostEditChangeProperty(ChangedEvent);
	Target->MarkPackageDirty();
	return true;
#else
	OutError = TEXT("에디터 전용 함수입니다.");
	return false;
#endif
}

FString UDRWardrobeEditorTools::ExportPropertyText(UObject* Target, FName PropertyName)
{
#if WITH_EDITOR
	if (!Target) return FString();

	const FProperty* Property = FindFProperty<FProperty>(Target->GetClass(), PropertyName);
	if (!Property) return FString();

	FString Text;
	Property->ExportTextItem_Direct(Text, Property->ContainerPtrToValuePtr<void>(Target), nullptr, Target, PPF_None);
	return Text;
#else
	return FString();
#endif
}

bool UDRWardrobeEditorTools::BuildWardrobeTabWidget(FString& OutLog)
{
#if WITH_EDITOR
	using namespace DRWardrobeEditor;
	OutLog.Reset();
	FLog Log{ OutLog };

	UWidgetBlueprint* Blueprint = Load<UWidgetBlueprint>(TabWBPPath, Log);
	UTexture2D* PillTexture = Load<UTexture2D>(PillTexPath, Log);
	UObject* LabelFont = Load<UObject>(LabelFontPath, Log);
	if (!Blueprint || !Blueprint->WidgetTree) return false;

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();

	// BP 로 만들다 만 배선은 C++ 로 옮겼다 — C++ 이 부르지 않는 함수/변수라 남겨 두면 헷갈린다 (T-13 · T-21)
	RemoveFunctionGraph(Blueprint, TEXT("SetSelected"), Log);
	RemoveVariable(Blueprint, TEXT("CosmeticCategory"), Log);
	RemoveVariable(Blueprint, TEXT("Label"), Log);

	if (UImage* Pill = Find<UImage>(Tree, TEXT("Image_Pill"), Log))
	{
		Pill->SetBrush(MakeImageBrush(PillTexture, FVector2D(124.0, 44.0)));
		Pill->SetVisibility(ESlateVisibility::Collapsed);	// 선택될 때 C++ 이 켠다
		Pill->bIsVariable = true;
		SetOverlaySlot(Pill, HAlign_Fill, VAlign_Fill);
	}

	if (UButton* Button = Find<UButton>(Tree, TEXT("Button_Tab"), Log))
	{
		MakeTransparentButton(Button);
		Button->bIsVariable = true;
		SetOverlaySlot(Button, HAlign_Fill, VAlign_Fill);
	}

	if (UTextBlock* Label = Find<UTextBlock>(Tree, TEXT("Text_Label"), Log))
	{
		// 실제 글자는 인스턴스의 LabelText 가 덮어쓴다. 대문자 (T-19)
		Label->SetText(FText::AsCultureInvariant(TEXT("HEAD")));
		Label->SetFont(MakeFont(LabelFont, 14.4f));	// 19.2px (D-24)
		Label->SetColorAndOpacity(FSlateColor(SRGB(0x66, 0x76, 0xB8)));
		Label->SetJustification(ETextJustify::Center);
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);	// 버튼 위에 있으므로 클릭을 통과시킨다 (T-41)
		Label->bIsVariable = true;
		// 시안은 글자가 필 중앙보다 약 1px 아래 — 위 여백 2 = 아래로 1px
		SetOverlaySlot(Label, HAlign_Center, VAlign_Center, FMargin(0.0f, 2.0f, 0.0f, 0.0f));
	}

	return CompileAndMark(Blueprint, Log);
#else
	return false;
#endif
}

bool UDRWardrobeEditorTools::BuildWardrobeSlotWidget(FString& OutLog)
{
#if WITH_EDITOR
	using namespace DRWardrobeEditor;
	OutLog.Reset();
	FLog Log{ OutLog };

	UWidgetBlueprint* Blueprint = Load<UWidgetBlueprint>(SlotWBPPath, Log);
	UTexture2D* NormalTexture = Load<UTexture2D>(SlotNormalTexPath, Log);
	UTexture2D* HoveredTexture = Load<UTexture2D>(SlotHoveredTexPath, Log);
	UTexture2D* SelectedTexture = Load<UTexture2D>(SlotSelectedTexPath, Log);
	UObject* BodyFont = Load<UObject>(BodyFontPath, Log);
	if (!Blueprint || !Blueprint->WidgetTree) return false;

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();

	// 옛 구조(버튼 스타일 통째 교체)의 BP 함수 — C++ 이 SetSkinViewModel → 그리기를 직접 한다 (T-13 · T-20)
	RemoveFunctionGraph(Blueprint, TEXT("Setup"), Log);

	// 루트 SizeBox (216 x 252)
	USizeBox* Root = Cast<USizeBox>(Tree->RootWidget);
	if (!Root)
	{
		DiscardRecursive(Tree, Tree->RootWidget);
		Root = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_Root"));
		Tree->RootWidget = Root;
	}
	Root->SetWidthOverride(216.0f);
	Root->SetHeightOverride(252.0f);

	// 옛 내용 전부 제거 → "투명 버튼 + 상태 이미지" 구조로 다시 만든다 (Overlay 는 뒤에 넣은 것이 위)
	DiscardRecursive(Tree, Root->GetContent());

	UOverlay* Overlay = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_Root"));
	Root->SetContent(Overlay);

	UImage* StateImage = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_State"));
	Overlay->AddChildToOverlay(StateImage);
	SetOverlaySlot(StateImage, HAlign_Fill, VAlign_Fill);
	StateImage->SetBrush(MakeImageBrush(NormalTexture, FVector2D(216.0, 252.0)));
	StateImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	StateImage->bIsVariable = true;

	UImage* Thumbnail = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Thumbnail"));
	Overlay->AddChildToOverlay(Thumbnail);
	SetOverlaySlot(Thumbnail, HAlign_Fill, VAlign_Fill, FMargin(16.0f));
	Thumbnail->SetVisibility(ESlateVisibility::Collapsed);	// 아이콘이 없으면 숨긴 채로 (D-18)
	Thumbnail->bIsVariable = true;

	UTextBlock* NameText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_Name"));
	Overlay->AddChildToOverlay(NameText);
	SetOverlaySlot(NameText, HAlign_Fill, VAlign_Center, FMargin(12.0f));
	NameText->SetText(FText::AsCultureInvariant(TEXT("Name")));
	NameText->SetFont(MakeFont(BodyFont, 13.5f));	// 한글 → Pretendard (BrunoAce 에는 한글이 없다)
	NameText->SetColorAndOpacity(FSlateColor(SRGB(0xDD, 0xF7, 0xFF)));
	NameText->SetJustification(ETextJustify::Center);
	NameText->SetAutoWrapText(true);
	NameText->SetVisibility(ESlateVisibility::Collapsed);
	NameText->bIsVariable = true;

	UImage* LockImage = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Lock"));
	Overlay->AddChildToOverlay(LockImage);
	SetOverlaySlot(LockImage, HAlign_Center, VAlign_Center);
	LockImage->SetVisibility(ESlateVisibility::Collapsed);	// 브러시가 생기면 C++ 이 잠긴 칸에만 켠다
	LockImage->bIsVariable = true;

	UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Slot"));
	Overlay->AddChildToOverlay(Button);
	SetOverlaySlot(Button, HAlign_Fill, VAlign_Fill);
	MakeTransparentButton(Button);
	Button->bIsVariable = true;

	if (!CompileAndMark(Blueprint, Log)) return false;

	UObject* CDO = GetCDO(Blueprint);
	SetObjectProperty(CDO, TEXT("StateNormalTexture"), NormalTexture, Log);
	SetObjectProperty(CDO, TEXT("StateHoveredTexture"), HoveredTexture, Log);
	SetObjectProperty(CDO, TEXT("StateSelectedTexture"), SelectedTexture, Log);
	return CompileAndMark(Blueprint, Log);	// CDO 를 고쳤으면 다시 컴파일 (T-32)
#else
	return false;
#endif
}

bool UDRWardrobeEditorTools::BuildCosmeticPreviewWidget(FString& OutLog)
{
#if WITH_EDITOR
	using namespace DRWardrobeEditor;
	OutLog.Reset();
	FLog Log{ OutLog };

	UWidgetBlueprint* Blueprint = Load<UWidgetBlueprint>(PreviewWBPPath, Log);
	UMaterialInterface* PreviewMaterial = Load<UMaterialInterface>(PreviewMaterialPath, Log);
	if (!Blueprint || !Blueprint->WidgetTree) return false;

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();

	UImage* PreviewImage = Cast<UImage>(Tree->FindWidget(TEXT("Img_Preview")));
	if (!PreviewImage || Tree->RootWidget != PreviewImage)
	{
		// 팩토리가 만든 기본 루트(캔버스 등)를 걷어 내고 이미지 하나를 루트로 둔다
		DiscardRecursive(Tree, Tree->RootWidget);
		if (PreviewImage) DiscardRecursive(Tree, PreviewImage);
		PreviewImage = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_Preview"));
		Tree->RootWidget = PreviewImage;
	}

	// ★Visible★ — Not Hit-Testable 이면 드래그 회전이 안 된다 (T-8). 머티리얼은 런타임에 C++ 이 물린다
	PreviewImage->SetVisibility(ESlateVisibility::Visible);
	PreviewImage->bIsVariable = true;

	if (!CompileAndMark(Blueprint, Log)) return false;

	SetObjectProperty(GetCDO(Blueprint), TEXT("PreviewMaterial"), PreviewMaterial, Log);
	return CompileAndMark(Blueprint, Log);	// CDO 를 고쳤으면 다시 컴파일 (T-32)
#else
	return false;
#endif
}

bool UDRWardrobeEditorTools::BuildWardrobeScreenWidget(FString& OutLog)
{
#if WITH_EDITOR
	using namespace DRWardrobeEditor;
	OutLog.Reset();
	FLog Log{ OutLog };

	UWidgetBlueprint* Blueprint = Load<UWidgetBlueprint>(ScreenWBPPath, Log);
	UWidgetBlueprint* TabBlueprint = Load<UWidgetBlueprint>(TabWBPPath, Log);
	UWidgetBlueprint* SlotBlueprint = Load<UWidgetBlueprint>(SlotWBPPath, Log);
	UWidgetBlueprint* PreviewBlueprint = Load<UWidgetBlueprint>(PreviewWBPPath, Log);
	UTexture2D* PanelTexture = Load<UTexture2D>(PanelTexPath, Log);
	UTexture2D* TabBarTexture = Load<UTexture2D>(TabBarTexPath, Log);
	UTexture2D* ClearAllTexture = Load<UTexture2D>(ClearAllTexPath, Log);
	UMaterialInterface* GlowMaterial = Load<UMaterialInterface>(GlowMIPath, Log);
	UMaterialInterface* CharBackMaterial = Load<UMaterialInterface>(CharBackMIPath, Log);
	UMaterialInterface* ScreenFXMaterial = Load<UMaterialInterface>(ScreenFXMIPath, Log);
	UMaterialInterface* PreviewMaterial = Load<UMaterialInterface>(PreviewMaterialPath, Log);
	UObject* TitleFont = Load<UObject>(TitleFontPath, Log);
	if (!Blueprint || !Blueprint->WidgetTree) return false;

	Blueprint->Modify();
	UWidgetTree* Tree = Blueprint->WidgetTree;
	Tree->Modify();

	UCanvasPanel* RootCanvas = Find<UCanvasPanel>(Tree, TEXT("CanvasPanel_Root"), Log);
	UCanvasPanel* Content = Find<UCanvasPanel>(Tree, TEXT("CanvasPanel_Content"), Log);
	if (!RootCanvas || !Content) return false;

	// ===== 1920x1080 디자인 좌표 — 2026-10-19 시안 실측 (Plan(옷장) D-24) =====
	const FAnchors TopLeft(0.0f, 0.0f);
	const FAnchors Center(0.5f, 0.5f);
	const FAnchors Stretch(0.0f, 0.0f, 1.0f, 1.0f);
	const FAnchors BottomRight(1.0f, 1.0f);
	const FVector2D NoAlign(0.0, 0.0);
	const FVector2D Mid(0.5, 0.5);

	// 1) 1차 배경 — 검정 60.8% 컬러 브러시. ★텍스처를 쓰지 않는다★ (알파 이중 적용 방지 — T-17)
	if (UImage* Dim = Find<UImage>(Tree, TEXT("Image_Dim"), Log))
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.608f));
		Dim->SetBrush(Brush);
		Dim->SetColorAndOpacity(FLinearColor::White);
		Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(Dim, Stretch, NoAlign, FVector2D::ZeroVector, FVector2D::ZeroVector);
	}

	// 2) 2차 배경 — 중앙 1601x901, Additive (Soft Light 근사 — D-23)
	if (UImage* Glow = Find<UImage>(Tree, TEXT("Image_BGGlow"), Log))
	{
		Glow->SetBrush(MakeImageBrush(GlowMaterial, FVector2D(1601.0, 901.0)));
		Glow->SetColorAndOpacity(FLinearColor::White);
		Glow->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(Glow, Center, Mid, FVector2D::ZeroVector, FVector2D(1601.0, 901.0));
	}

	// 3) 캐릭터 back — (895,2) 868x1076, Additive + 청색 틴트 (Lighter Color 근사)
	if (UImage* CharBack = Find<UImage>(Tree, TEXT("Image_CharBack"), Log))
	{
		CharBack->SetBrush(MakeImageBrush(CharBackMaterial, FVector2D(868.0, 1076.0)));
		CharBack->SetColorAndOpacity(FLinearColor::White);
		CharBack->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(CharBack, TopLeft, NoAlign, FVector2D(895.0, 2.0), FVector2D(868.0, 1076.0));
	}

	// 4) 패널 — (150,146) 820x839 (예전 (151,147) 은 시안 배율 보정 누락)
	if (UImage* Panel = Find<UImage>(Tree, TEXT("Image_PanelFrame"), Log))
	{
		Panel->SetBrush(MakeImageBrush(PanelTexture, FVector2D(820.0, 839.0)));
		Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(Panel, TopLeft, NoAlign, FVector2D(150.0, 146.0), FVector2D(820.0, 839.0));
	}

	// 5) 콘텐츠 묶음 (전체 늘이기)
	PlaceOnCanvas(Content, Stretch, NoAlign, FVector2D::ZeroVector, FVector2D::ZeroVector);

	// 5-1) 3D 프리뷰 — (938,0) 781x1080. ★이름 Preview_Character 고정★ (BindWidgetOptional)
	UUserWidget* Preview = Cast<UUserWidget>(Tree->FindWidget(TEXT("Preview_Character")));
	if (!Preview && PreviewBlueprint && PreviewBlueprint->GeneratedClass)
	{
		Preview = Tree->ConstructWidget<UUserWidget>(TSubclassOf<UUserWidget>(PreviewBlueprint->GeneratedClass.Get()), TEXT("Preview_Character"));
		Content->InsertChildAt(0, Preview);	// 제목 · 탭보다 뒤에
		Log.Add(TEXT("Preview_Character 추가"));
	}
	if (Preview)
	{
		Preview->bIsVariable = true;
		PlaceOnCanvas(Preview, TopLeft, NoAlign, FVector2D(938.0, 0.0), FVector2D(781.0, 1080.0));
		// ★인스턴스에도 직접 넣는다★ — 프리뷰 CDO 가 비어 있던 때 만든 인스턴스는 None 을 따로 저장해 두어
		// CDO 를 고쳐도 따라가지 않는다 (T-32)
		SetObjectProperty(Preview, TEXT("PreviewMaterial"), PreviewMaterial, Log);
	}

	// 5-2) 제목 — 학교안심 운동장 L 55px(=41.25) @ (202,54), #DDF7FF. Bruno Ace 가 아니다 (D-24)
	if (UTextBlock* Title = Find<UTextBlock>(Tree, TEXT("Text_Title"), Log))
	{
		Title->SetText(FText::AsCultureInvariant(TEXT("WARDROBE")));
		// 자간은 0 그대로 — 시안보다 끝 글자가 3~4px 왼쪽이지만 Slate 자간은 정수 px 로 잘려 1px 미만은 못 넣는다 (T-35)
		Title->SetFont(MakeFont(TitleFont, 41.25f));
		Title->SetColorAndOpacity(FSlateColor(SRGB(0xDD, 0xF7, 0xFF)));
		Title->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(Title, TopLeft, NoAlign, FVector2D(202.0, 54.0), FVector2D::ZeroVector, true);
	}

	// 5-3) 탭 바 — (201,139) 496x44, 탭 4개 (124 씩)
	if (UOverlay* TabBar = Find<UOverlay>(Tree, TEXT("Overlay_TabBar"), Log))
	{
		PlaceOnCanvas(TabBar, TopLeft, NoAlign, FVector2D(201.0, 139.0), FVector2D(496.0, 44.0));
	}
	if (UImage* TabBarBackground = Find<UImage>(Tree, TEXT("Image_TabBarBG"), Log))
	{
		TabBarBackground->SetBrush(MakeImageBrush(TabBarTexture, FVector2D(496.0, 44.0)));
		TabBarBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetOverlaySlot(TabBarBackground, HAlign_Fill, VAlign_Fill);
	}
	if (UHorizontalBox* Tabs = Find<UHorizontalBox>(Tree, TEXT("HorizontalBox_Tabs"), Log))
	{
		SetOverlaySlot(Tabs, HAlign_Fill, VAlign_Fill);

		// 다시 돌려도 같은 결과가 나오게 기존 탭을 걷어 내고 새로 넣는다
		TArray<UWidget*> OldTabs = Tabs->GetAllChildren();
		for (UWidget* OldTab : OldTabs)
		{
			DiscardRecursive(Tree, OldTab);
		}

		struct FTabSpec { EDRCosmeticCategory Category; const TCHAR* Label; const TCHAR* Name; };
		const FTabSpec Specs[] = {
			{ EDRCosmeticCategory::Head, TEXT("HEAD"), TEXT("Tab_Head") },
			{ EDRCosmeticCategory::Face, TEXT("FACE"), TEXT("Tab_Face") },
			{ EDRCosmeticCategory::Body, TEXT("BODY"), TEXT("Tab_Body") },
			{ EDRCosmeticCategory::Tail, TEXT("TAIL"), TEXT("Tab_Tail") },
		};

		if (TabBlueprint && TabBlueprint->GeneratedClass)
		{
			for (const FTabSpec& Spec : Specs)
			{
				UUserWidget* Tab = Tree->ConstructWidget<UUserWidget>(TSubclassOf<UUserWidget>(TabBlueprint->GeneratedClass.Get()), Spec.Name);
				if (UHorizontalBoxSlot* BoxSlot = Tabs->AddChildToHorizontalBox(Tab))
				{
					BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));	// 각 124 → 합 496
					BoxSlot->SetHorizontalAlignment(HAlign_Left);
					BoxSlot->SetVerticalAlignment(VAlign_Fill);
				}
				if (UDRWardrobeTabWidget* TabWidget = Cast<UDRWardrobeTabWidget>(Tab))
				{
					TabWidget->Category = Spec.Category;	// ★탭마다 다르게★ (T-21)
					TabWidget->LabelText = FText::AsCultureInvariant(Spec.Label);
				}
			}
			Log.Add(TEXT("탭 4개 배치 (HEAD · FACE · BODY · TAIL)"));
		}
	}

	// 5-4) 칸 그리드 — 칸 왼쪽 위가 (224,227) 이 되도록 슬롯 패딩 6 만큼 바깥으로 (218,221) 684x689.
	//      UniformGridPanel 의 패딩은 칸마다 사방에 붙는다 → 3 x (216 + 12) = 684
	if (UScrollBox* Scroll = Find<UScrollBox>(Tree, TEXT("ScrollBox_Slots"), Log))
	{
		PlaceOnCanvas(Scroll, TopLeft, NoAlign, FVector2D(218.0, 221.0), FVector2D(684.0, 689.0));
		Scroll->SetOrientation(Orient_Vertical);
		Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);	// D-18
		Scroll->SetAllowOverscroll(false);
		Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	}
	if (UUniformGridPanel* Grid = Find<UUniformGridPanel>(Tree, TEXT("UniformGridPanel_Slots"), Log))
	{
		Grid->SetSlotPadding(FMargin(6.0f));
		Grid->bIsVariable = true;	// BindWidgetOptional — C++ 이 칸을 채운다
	}

	// 5-5) Clear All — 화면 우하단 (업그레이드 화면 키 힌트 바와 같은 앵커 · 오프셋, D-17)
	UButton* ClearAll = Cast<UButton>(Tree->FindWidget(TEXT("Btn_ClearAll")));
	if (!ClearAll)
	{
		ClearAll = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Btn_ClearAll"));
		Content->AddChild(ClearAll);
		Log.Add(TEXT("Btn_ClearAll 추가"));
	}
	{
		FButtonStyle Style = ClearAll->GetStyle();
		const FSlateBrush Normal = MakeImageBrush(ClearAllTexture, FVector2D(112.0, 117.0));
		FSlateBrush Pressed = Normal;
		Pressed.TintColor = FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));
		Style.SetNormal(Normal).SetHovered(Normal).SetPressed(Pressed).SetDisabled(Normal);
		Style.SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f));
		ClearAll->SetStyle(Style);
		ClearAll->SetToolTipText(NSLOCTEXT("DRWardrobe", "ClearAllTooltip", "전부 벗기 (C)"));
		ClearAll->bIsVariable = true;
		PlaceOnCanvas(ClearAll, BottomRight, FVector2D(1.0, 1.0), FVector2D(-100.0, -60.0), FVector2D(112.0, 117.0));
	}

	// 6) 화면효과 — 중앙 1613x914 (원본의 92% — 시안 스캔라인 간격 실측), Modulate. ★클릭을 먹지 않게★
	if (UImage* ScreenFX = Find<UImage>(Tree, TEXT("Image_ScreenFX"), Log))
	{
		ScreenFX->SetBrush(MakeImageBrush(ScreenFXMaterial, FVector2D(1613.0, 914.0)));
		ScreenFX->SetColorAndOpacity(FLinearColor::White);
		ScreenFX->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(ScreenFX, Center, Mid, FVector2D::ZeroVector, FVector2D(1613.0, 914.0));
	}

	if (!CompileAndMark(Blueprint, Log)) return false;

	if (SlotBlueprint)
	{
		SetObjectProperty(GetCDO(Blueprint), TEXT("SlotWidgetClass"), SlotBlueprint->GeneratedClass.Get(), Log);
	}
	return CompileAndMark(Blueprint, Log);	// CDO 를 고쳤으면 다시 컴파일 (T-32)
#else
	return false;
#endif
}

bool UDRWardrobeEditorTools::StartPlayInNewWindow(int32 Width, int32 Height, int32 NumPlayers)
{
#if WITH_EDITOR
	if (!GEditor || GEditor->IsPlaySessionInProgress() || Width <= 0 || Height <= 0 || NumPlayers <= 0) return false;

	// 에디터 설정(CDO)은 그대로 두고 복사본만 고쳐 넘긴다 — RequestPlaySession 이 한 번 더 복사해 붙잡아 둔다
	ULevelEditorPlaySettings* PlaySettings = DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage());
	PlaySettings->NewWindowWidth = Width;
	PlaySettings->NewWindowHeight = Height;
	PlaySettings->CenterNewWindow = true;
	PlaySettings->SetPlayNumberOfClients(NumPlayers);
	if (NumPlayers > 1)
	{
		// 복제 확인용 — 첫 창이 리슨 서버(호스트), 나머지가 클라이언트
		PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
		PlaySettings->SetRunUnderOneProcess(true);
	}

	// DestinationSlateViewport 를 비워 두면 레벨 뷰포트 대신 새 창으로 뜬다
	FRequestPlaySessionParams Params;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;
	Params.SessionDestination = EPlaySessionDestinationType::InProcess;
	Params.EditorPlaySettings = PlaySettings;
	GEditor->RequestPlaySession(Params);

	UE_LOG(LogDR, Display, TEXT("[WardrobeEditor] PIE 새 창 요청 %dx%d · %d명%s"), Width, Height, NumPlayers,
		NumPlayers > 1 ? TEXT(" (리슨 서버)") : TEXT(""));
	return true;
#else
	return false;
#endif
}

bool UDRWardrobeEditorTools::SetMeshSocket(USkeletalMesh* Mesh, FName SocketName, FName BoneName,
	FVector Location, FRotator Rotation, FVector Scale)
{
#if WITH_EDITOR
	if (!Mesh || SocketName.IsNone() || BoneName.IsNone()) return false;

	if (Mesh->GetRefSkeleton().FindBoneIndex(BoneName) == INDEX_NONE)
	{
		UE_LOG(LogDR, Warning, TEXT("[WardrobeEditor] %s 에 '%s' 뼈가 없습니다."), *Mesh->GetName(), *BoneName.ToString());
		return false;
	}

	Mesh->Modify();

	// 메시 전용 소켓 목록에서만 찾는다 — FindSocket 은 스켈레톤 소켓까지 보는데, 그쪽은 다른 메시와 공유라 건드리지 않는다
	USkeletalMeshSocket* Socket = nullptr;
	for (USkeletalMeshSocket* Existing : Mesh->GetMeshOnlySocketList())
	{
		if (Existing && Existing->SocketName == SocketName)
		{
			Socket = Existing;
			break;
		}
	}
	if (!Socket)
	{
		Socket = NewObject<USkeletalMeshSocket>(Mesh);
		Socket->SocketName = SocketName;
		Socket->BoneName = BoneName;	// ★AddSocket 이 뼈 이름을 검사하므로 먼저 넣는다★ — 비어 있으면 조용히 안 붙는다
		Mesh->AddSocket(Socket, /*bAddToSkeleton*/ false);

		if (!Mesh->GetMeshOnlySocketList().Contains(Socket))
		{
			UE_LOG(LogDR, Warning, TEXT("[WardrobeEditor] %s 에 소켓 %s 를 붙이지 못했습니다."), *Mesh->GetName(), *SocketName.ToString());
			return false;
		}
	}

	Socket->Modify();
	Socket->BoneName = BoneName;
	Socket->RelativeLocation = Location;
	Socket->RelativeRotation = Rotation;
	Socket->RelativeScale = Scale;

	// FindSocket 은 이름 캐시를 본다 — AddSocket 이 캐시를 다시 만들지 않으므로 직접 (안 하면 다시 열 때까지 "없는 소켓")
	Mesh->RebuildSocketMap();
	Mesh->MarkPackageDirty();
	UE_LOG(LogDR, Display, TEXT("[WardrobeEditor] %s 소켓 %s → %s (%s / %s / %s)"), *Mesh->GetName(), *SocketName.ToString(),
		*BoneName.ToString(), *Location.ToString(), *Rotation.ToString(), *Scale.ToString());
	return true;
#else
	return false;
#endif
}

bool UDRWardrobeEditorTools::CallOnNextTick(UObject* Target, const FString& FunctionAndArgs)
{
#if WITH_EDITOR
	if (!Target || FunctionAndArgs.IsEmpty()) return false;

	TWeakObjectPtr<UObject> WeakTarget(Target);
	const FString Command = FunctionAndArgs;

	// 코어 티커는 Python 호출 밖(엔진 루프)에서 돈다 — GAllowActorScriptExecutionInEditor 가 꺼진 상태라 RPC 가 원격으로 간다
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakTarget, Command](float)
	{
		if (UObject* Object = WeakTarget.Get())
		{
			const bool bOk = Object->CallFunctionByNameWithArguments(*Command, *GLog, nullptr, /*bForceCallWithNonExec*/ true);
			UE_LOG(LogDR, Display, TEXT("[WardrobeEditor] 다음 틱 호출 %s : %s → %s"),
				*Object->GetName(), *Command, bOk ? TEXT("성공") : TEXT("★실패★"));
		}
		return false;	// 한 번만
	}));
	return true;
#else
	return false;
#endif
}
