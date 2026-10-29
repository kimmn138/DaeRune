// Copyright DaeRune


#include "DRCharacterInfoEditorTools.h"

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
#include "Components/RichTextBlock.h"
#include "Components/RichTextBlockImageDecorator.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "DaeRune/DRLogChannels.h"
#include "DREditorToolCommon.h"
#endif

#if WITH_EDITOR
namespace DRCharacterInfoEditor
{
	// 세 도구 공용 헬퍼 (DREditorToolCommon.h)
	using DREditorToolCommon::SRGB;
	using DREditorToolCommon::PickTypeface;
	using DREditorToolCommon::DiscardRecursive;
	using DREditorToolCommon::Load;
	using DREditorToolCommon::CompileAndMark;
	using DREditorToolCommon::SetObjectProperty;

	const TCHAR* TitleFontPath = TEXT("/Game/DaeRuneAssets/UI/Font/HakgyoansimUndongjangL_Font.HakgyoansimUndongjangL_Font");
	const TCHAR* BodyFontPath = TEXT("/Game/DaeRuneAssets/UI/Font/HakgyoansimUndongjangL_Font.HakgyoansimUndongjangL_Font");
	const TCHAR* NumberFontPath = TEXT("/Game/DaeRuneAssets/UI/Font/BrunoAceSC-Regular_Font.BrunoAceSC-Regular_Font");

	// ===== 배경 그림 배치 =====
	// 예전 그림(테두리만, 4386×2256)을 1536×790 에 그렸다 → 배율 1536/4386.
	// 새 그림(5400×2829)은 같은 테두리에 투명 여백(왼쪽 494 · 위 302)을 붙인 것이라, 같은 배율이면 예전 글자 좌표에 여백만 더하면 된다.
	const float Scale = 1536.0f / 4386.0f;
	const FVector2D ImageSize(5400.0f * Scale, 2829.0f * Scale);	// 1891 × 991 — 화면(1920×1080) 가운데에 놓는다
	const FVector2D OldOrigin(494.0f * Scale, 302.0f * Scale);		// 예전 패널 (0,0) 이 새 패널에서 놓이는 곳

	FVector2D FromOld(float X, float Y) { return OldOrigin + FVector2D(X, Y); }

	// 예전 WBP 의 색(선형값 그대로)
	const FLinearColor BodyColor(0.429f, 0.947f, 1.0f);		// 부제 · 설명 · 난이도
	const FLinearColor SkillTitleColor(0.515f, 0.871f, 1.0f);	// 스킬 이름
	const FLinearColor NumberColor = FLinearColor::White;		// 체력 · 물 · 물 사용량


	// 캐릭터 이름 — 예전 그림에 박혀 있던 글자의 색(실측)
	const FLinearColor TitleColor = SRGB(158, 237, 255);
	const FLinearColor BadgeColor = SRGB(255, 240, 74);	// 자판기 "VENDING" 노란 딱지
	const FLinearColor BadgeTextColor = SRGB(69, 45, 23);

	// 조작 버튼 그림. 마우스 · E 는 126×159, Q 는 112×117 — 버튼 몸통 높이가 같아 보이게 배율을 따로 준다
	struct FKeyIcon
	{
		const TCHAR* Path;
		FVector2D Size;
	};
	const FKeyIcon KeyLMB{ TEXT("/Game/DaeRuneAssets/UI/SkillIcon/SkillIcon_LMB.SkillIcon_LMB"), FVector2D(126.0f, 159.0f) * (25.0f / 159.0f) };
	const FKeyIcon KeyRMB{ TEXT("/Game/DaeRuneAssets/UI/SkillIcon/SkillIcon_RMB.SkillIcon_RMB"), FVector2D(126.0f, 159.0f) * (25.0f / 159.0f) };
	const FKeyIcon KeyE{ TEXT("/Game/DaeRuneAssets/UI/SkillIcon/SkillIcon_E.SkillIcon_E"), FVector2D(126.0f, 159.0f) * (25.0f / 159.0f) };
	const FKeyIcon KeyQ{ TEXT("/Game/DaeRuneAssets/UI/SkillIcon/SkillIcon_Q.SkillIcon_Q"), FVector2D(112.0f, 117.0f) * (19.8f / 112.0f) };

	// 물 사용량 칸 그림 (133×38) — 예전 그림의 칸 높이 26 에 맞춘다
	const TCHAR* WaterCostPath = TEXT("/Game/DaeRuneAssets/UI/Tutorial/CharacterInfo-Water.CharacterInfo-Water");
	const FVector2D WaterCostSize = FVector2D(133.0f, 38.0f) * (26.0f / 38.0f);

	// ===== 캐릭터별 사양 =====
	// 글자는 예전 WBP 에서 이름(OldName)으로 찾아 FText 째로 옮긴다(번역 키 유지). 없을 때만 Korean 으로 새 키를 만든다.
	struct FTextSpec
	{
		const TCHAR* OldName = nullptr;
		const TCHAR* Korean = nullptr;
	};

	struct FSkillRowSpec
	{
		const FKeyIcon* Key = nullptr;	// nullptr = 패시브(버튼 없음)
		float TitleY = 0.0f;			// 예전 패널 좌표
		float DescY = 0.0f;
		float TitleSize = 15.75f;
		FTextSpec Title;
		FTextSpec Desc;
		FTextSpec Cost;					// Korean == nullptr 이고 OldName 도 없으면 물 사용량 칸 없음
	};

	struct FCharacterSpec
	{
		const TCHAR* Name;
		const TCHAR* WidgetPath;
		const TCHAR* BackgroundPath;
		const TCHAR* TitleBadge;		// 노란 딱지 글자 (nullptr = 없음)
		const TCHAR* TitleText;
		float TitleMaxWidth;			// 배경의 줄무늬 장식 앞까지 — 넘치면 가로로만 줄인다
		FTextSpec Health;
		FTextSpec Water;
		FTextSpec Subtitle;
		FTextSpec Difficulty;
		FSkillRowSpec Rows[3];
	};

	// 예전 두 WBP 의 행 좌표(같다): 스킬 이름 y 292 · 419 · 548, 설명 y 334 · 457 · 587
	const FCharacterSpec Specs[] =
	{
		{
			TEXT("Gardener"),
			TEXT("/Game/Blueprints/UI/CharacterInfo/WBP_GardenInfo.WBP_GardenInfo"),
			TEXT("/Game/DaeRuneAssets/UI/Tutorial/UI-CharacterInfo-Gardener.UI-CharacterInfo-Gardener"),
			nullptr, TEXT("GARDENER"), 282.0f,
			{ TEXT("텍스트_TopEnergy400"), TEXT("400") },
			{ TEXT("텍스트_TopWater100"), TEXT("100") },
			{ TEXT("텍스트_Subtitle") },
			{ nullptr, TEXT("조작 난이도: 쉬움") },
			{
				{ &KeyLMB, 292.0f, 334.0f, 17.25f, { TEXT("텍스트_BasicTitle") }, { TEXT("텍스트_BasicDesc") }, {} },
				{ &KeyRMB, 419.0f, 457.0f, 15.75f, { TEXT("텍스트_WaterTitle") }, { TEXT("텍스트_WaterDesc") }, { TEXT("텍스트_Cost5"), TEXT("5") } },
				{ &KeyE, 548.0f, 587.0f, 15.75f, { TEXT("텍스트_SeedTitle") }, { TEXT("텍스트_SeedDesc") }, { TEXT("텍스트_Cost50"), TEXT("30") } },
			},
		},
		{
			TEXT("VendingMachine"),
			TEXT("/Game/Blueprints/UI/CharacterInfo/WBP_VendingInfo.WBP_VendingInfo"),
			TEXT("/Game/DaeRuneAssets/UI/Tutorial/UI-CharacterInfo-VendingMachine.UI-CharacterInfo-VendingMachine"),
			TEXT("VENDING"), TEXT("MACHINE"), 567.0f,
			{ TEXT("Text_EnergyAmount"), TEXT("400") },
			{ TEXT("Text_WaterAmount"), TEXT("100") },
			{ TEXT("Text_Subtitle") },
			{ nullptr, TEXT("조작 난이도: 보통") },
			{
				{ &KeyLMB, 292.0f, 334.0f, 17.25f, { TEXT("Text_BasicTitle") }, { TEXT("Text_BasicDesc") }, {} },
				{ nullptr, 419.0f, 457.0f, 15.75f, { TEXT("Text_PassiveTitle") }, { TEXT("Text_PassiveDesc") }, {} },
				{ &KeyE, 548.0f, 587.0f, 15.75f, { TEXT("Text_BuffTitle") }, { TEXT("Text_BuffDesc") }, { TEXT("Text_Cost50"), TEXT("50") } },
			},
		},
		{
			// 배경 그림의 아이콘 순서: 공기탄 · 버스트 · 돌진. 패시브(탑승)는 부제 자리에 둔다
			// 설명 속 <img id="…"/> 는 조작 버튼 그림으로 바뀐다(RichText, KeyImageIds)
			TEXT("RobotVacuum"),
			TEXT("/Game/Blueprints/UI/CharacterInfo/WBP_VacuumInfo.WBP_VacuumInfo"),
			TEXT("/Game/DaeRuneAssets/UI/Tutorial/UI-CharacterInfo-VecuumCleaner.UI-CharacterInfo-VecuumCleaner"),
			nullptr, TEXT("VACUUM"), 282.0f,
			{ nullptr, TEXT("100") },
			{ nullptr, TEXT("100") },
			{ nullptr, TEXT("패시브: 아군이 등에 올라탈 수 있습니다.") },
			{ nullptr, TEXT("조작 난이도: 어려움") },
			{
				{ &KeyLMB, 292.0f, 334.0f, 17.25f, { nullptr, TEXT("공기탄") },
					{ nullptr, TEXT("적에게 공기탄을 발사합니다. <img id=\"LMB\"/>을 길게 누르면 피해량이 증가합니다.") }, {} },
				{ &KeyQ, 419.0f, 457.0f, 15.75f, { nullptr, TEXT("버스트") },
					{ nullptr, TEXT("공기를 폭발시켜 위로 솟구치며, 주변 적에게 피해를 줍니다.") }, { nullptr, TEXT("30") } },
				{ &KeyRMB, 548.0f, 587.0f, 15.75f, { nullptr, TEXT("돌진") },
					{ nullptr, TEXT("적을 관통하며 돌진해 피해를 줍니다. <img id=\"RMB\"/>을 길게 누르면 피해량과 속도가 증가합니다.") },
					{ nullptr, TEXT("20") } },
			},
		},
		{
			// 배경 그림의 아이콘 순서: 가방(기본 공격 LMB) · 종(도발 E) · 모자(모자 던지기 RMB)
			TEXT("BellBoy"),
			TEXT("/Game/Blueprints/UI/CharacterInfo/WBP_BellBoyInfo.WBP_BellBoyInfo"),
			TEXT("/Game/DaeRuneAssets/UI/Tutorial/UI-CharacterInfo-BellBoy.UI-CharacterInfo-BellBoy"),
			nullptr, TEXT("BELLBOY"), 282.0f,
			{ nullptr, TEXT("300") },
			{ nullptr, TEXT("100") },
			{ nullptr, TEXT("적의 시선을 끌고 아군을 보호하는 근거리 지원형 로봇입니다.") },
			{ nullptr, TEXT("조작 난이도: 보통") },
			{
				{ &KeyLMB, 292.0f, 334.0f, 15.75f, { nullptr, TEXT("스킬 - 짐 조심하세요!") },
					{ nullptr, TEXT("가방을 휘둘러 전방의 적에게 피해를 줍니다.") }, {} },
				{ &KeyE, 419.0f, 457.0f, 15.75f, { nullptr, TEXT("스킬 - 이쪽입니다, 손님!") },
					{ nullptr, TEXT("주변의 적을 약화시키고 자신을 공격하도록 도발합니다.") }, { nullptr, TEXT("60") } },
				{ &KeyRMB, 548.0f, 587.0f, 15.75f, { nullptr, TEXT("스킬 - 모자 서비스") },
					{ nullptr, TEXT("모자를 던집니다. 적에게 적중하면 피해를 주고, 아군에게 적중하면 다음 피해를 한 번 막는 보호막을 부여합니다.") },
					{ nullptr, TEXT("20") } },
			},
		},
	};

	struct FLog
	{
		FString& Out;
		bool bError = false;
		void Add(const FString& Line)
		{
			Out += Line + TEXT("\n");
			UE_LOG(LogDR, Display, TEXT("[CharacterInfoEditor] %s"), *Line);
		}
		void Error(const FString& Line)
		{
			bError = true;
			Add(TEXT("★") + Line + TEXT("★"));
		}
	};



	template <typename T>
	T* Make(UWidgetTree* Tree, const FString& Name)
	{
		T* Widget = Tree->ConstructWidget<T>(T::StaticClass(), FName(*Name));
		Widget->bIsVariable = false;
		Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Widget;
	}

	UCanvasPanelSlot* AddToCanvas(UCanvasPanel* Canvas, UWidget* Child, const FVector2D& Position, const FVector2D& Size, bool bAutoSize,
		const FVector2D& Alignment = FVector2D::ZeroVector)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Child);
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		CanvasSlot->SetAlignment(Alignment);
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetAutoSize(bAutoSize);
		if (!bAutoSize)
		{
			CanvasSlot->SetSize(Size);
		}
		return CanvasSlot;
	}

	UHorizontalBoxSlot* AddToHBox(UHorizontalBox* Box, UWidget* Child, float LeftPadding)
	{
		UHorizontalBoxSlot* BoxSlot = Box->AddChildToHorizontalBox(Child);
		BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		BoxSlot->SetHorizontalAlignment(HAlign_Left);
		BoxSlot->SetVerticalAlignment(VAlign_Center);
		BoxSlot->SetPadding(FMargin(LeftPadding, 0.0f, 0.0f, 0.0f));
		return BoxSlot;
	}

	FSlateBrush MakeImageBrush(UObject* Resource, const FVector2D& Size)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetResourceObject(Resource);
		Brush.ImageSize = Size;
		return Brush;
	}

	struct FBuilder
	{
		UWidgetBlueprint* Blueprint = nullptr;
		UWidgetTree* Tree = nullptr;
		FLog& Log;
		TMap<FName, FText> OldTexts;	// 다시 짓기 전 트리의 글자 (위젯 이름 → FText)
		UObject* TitleFont = nullptr;
		UObject* BodyFont = nullptr;
		UObject* NumberFont = nullptr;
		TSubclassOf<URichTextBlockDecorator> KeyDecorator;

		// 새 이름 → 예전 이름 → Korean(새 키) 순으로 찾는다.
		// 표에 Korean 이 있으면 그것이 정본이다 — 위젯의 원문과 다르면(문구를 고쳤으면) 새 키로 바꾼다(옛 키는 다음 Gather 에서 빠진다)
		FText ResolveText(const FString& NewName, const FTextSpec& Spec, bool bLocalizable)
		{
			if (const FText* Found = OldTexts.Find(FName(*NewName)))
			{
				const FString* Source = FTextInspector::GetSourceString(*Found);
				if (!Spec.Korean || (Source && *Source == Spec.Korean))
				{
					return *Found;
				}
				Log.Add(FString::Printf(TEXT("  %s: 문구 바뀜 \"%s\""), *NewName, Source ? **Source : TEXT("")));
				return MakeNewText(NewName, Spec.Korean, bLocalizable);
			}
			if (Spec.OldName)
			{
				if (const FText* Found = OldTexts.Find(FName(Spec.OldName)))
				{
					Log.Add(FString::Printf(TEXT("  %s ← 예전 %s \"%s\""), *NewName, Spec.OldName, *Found->ToString().Left(30)));
					return *Found;
				}
				Log.Error(FString::Printf(TEXT("예전 글자 없음: %s"), Spec.OldName));
			}
			if (!Spec.Korean)
			{
				return FText::GetEmpty();
			}
			return MakeNewText(NewName, Spec.Korean, bLocalizable);
		}

		FText MakeNewText(const FString& NewName, const TCHAR* Korean, bool bLocalizable)
		{
			if (!bLocalizable)
			{
				return FText::AsCultureInvariant(Korean);
			}
			// 디자이너에서 새 TextBlock 에 글자를 칠 때와 같은 키(패키지 네임스페이스 + 새 GUID)
			const FString Namespace = TextNamespaceUtil::BuildFullNamespace(FString(), TextNamespaceUtil::EnsurePackageNamespace(Blueprint));
			Log.Add(FString::Printf(TEXT("  %s: 새 글자 \"%s\""), *NewName, Korean));
			return FText::ChangeKey(FTextKey(Namespace), FTextKey(FGuid::NewGuid().ToString()), FText::FromString(Korean));
		}

		// 스킬 설명 — RichText 라 <img id="LMB"/> 같은 태그가 조작 버튼 그림이 된다(KeyDecorator).
		// 너비를 고정하고 자동 줄바꿈한다 (번역으로 길어지면 아래로 늘어난다)
		UWidget* MakeRichDesc(const FString& Name, const FText& Text, float Size, float Width)
		{
			USizeBox* Box = Make<USizeBox>(Tree, TEXT("SizeBox_") + Name.RightChop(5));
			Box->SetWidthOverride(Width);
			URichTextBlock* RichText = Make<URichTextBlock>(Tree, Name);
			RichText->SetText(Text);
			FTextBlockStyle Style;
			Style.SetFont(FSlateFontInfo(BodyFont, Size, PickTypeface(BodyFont)));
			Style.SetColorAndOpacity(FSlateColor(BodyColor));
			RichText->SetDefaultTextStyle(Style);
			RichText->SetAutoWrapText(true);
			if (KeyDecorator)
			{
				RichText->SetDecorators({ KeyDecorator });
			}
			Box->AddChild(RichText);
			return Box;
		}

		UTextBlock* MakeText(const FString& Name, const FText& Text, UObject* Font, float Size, const FLinearColor& Color)
		{
			UTextBlock* TextBlock = Make<UTextBlock>(Tree, Name);
			TextBlock->SetText(Text);
			TextBlock->SetFont(FSlateFontInfo(Font, Size, PickTypeface(Font)));
			TextBlock->SetColorAndOpacity(FSlateColor(Color));
			return TextBlock;
		}

		// 너비를 고정하고 자동 줄바꿈하는 글자 (번역으로 길어지면 아래로 늘어난다)
		UWidget* MakeWrappedText(const FString& Name, const FText& Text, float Size, float Width)
		{
			USizeBox* Box = Make<USizeBox>(Tree, TEXT("SizeBox_") + Name.RightChop(5));
			Box->SetWidthOverride(Width);
			UTextBlock* TextBlock = MakeText(Name, Text, BodyFont, Size, BodyColor);
			TextBlock->SetAutoWrapText(true);
			Box->AddChild(TextBlock);
			return Box;
		}

		// 캐릭터 이름 — 줄무늬 장식 앞(TitleMaxWidth)까지. 넘치면 렌더 트랜스폼으로 ★가로로만★ 줄인다
		// (세로는 그대로 → 예전 그림의 좁은 글꼴처럼 보인다). ScaleBox 의 Fill 은 칸만 늘리고 줄이지 않아서 못 쓴다.
		// 이름은 언어와 상관없이 고정이라 지을 때 한 번 재면 된다.
		UWidget* MakeTitle(const FCharacterSpec& Spec)
		{
			UHorizontalBox* Row = Make<UHorizontalBox>(Tree, TEXT("HBox_Title"));

			const float TitleSize = 60.0f;
			const FSlateFontInfo MeasureFont(TitleFont, TitleSize, PickTypeface(TitleFont));
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			float Width = Measure->Measure(FString(Spec.TitleText), MeasureFont).X;
			if (Spec.TitleBadge)
			{
				Width += Measure->Measure(FString(Spec.TitleBadge), MeasureFont).X + 24.0f + 16.0f;	// 딱지 좌우 여백 + 간격
			}
			const float ScaleX = FMath::Min(1.0f, Spec.TitleMaxWidth / FMath::Max(Width, 1.0f));
			Row->SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
			Row->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(ScaleX, 1.0f), FVector2D::ZeroVector, 0.0f));
			Log.Add(FString::Printf(TEXT("  이름 너비 %.0f → 가로 배율 %.2f (최대 %.0f)"), Width, ScaleX, Spec.TitleMaxWidth));

			if (Spec.TitleBadge)
			{
				UBorder* Badge = Make<UBorder>(Tree, TEXT("Border_TitleBadge"));
				FSlateBrush BadgeBrush;
				BadgeBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
				BadgeBrush.TintColor = FSlateColor(BadgeColor);
				BadgeBrush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(10.0f, 10.0f, 10.0f, 10.0f));
				BadgeBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
				Badge->SetBrush(BadgeBrush);
				Badge->SetPadding(FMargin(12.0f, 0.0f, 12.0f, 0.0f));
				Badge->SetContent(MakeText(TEXT("Text_TitleBadge"), FText::AsCultureInvariant(Spec.TitleBadge), TitleFont, TitleSize, BadgeTextColor));
				AddToHBox(Row, Badge, 0.0f);
			}

			// 이름은 예전 그림처럼 언어와 상관없이 영어로 둔다(번역 수집 제외)
			UTextBlock* Name = MakeText(TEXT("Text_Title"), FText::AsCultureInvariant(Spec.TitleText), TitleFont, TitleSize, TitleColor);
			Name->SetShadowOffset(FVector2D(0.0f, 3.0f));
			Name->SetShadowColorAndOpacity(SRGB(4, 18, 42, 0.6f));
			AddToHBox(Row, Name, Spec.TitleBadge ? 16.0f : 0.0f);
			return Row;
		}

		// 스킬 이름 줄: [이름][조작 버튼][물 사용량 칸] — 이름 너비를 따라 버튼이 밀려난다
		UWidget* MakeSkillHeader(int32 Index, const FSkillRowSpec& Row)
		{
			const FString N = FString::FromInt(Index);
			UHorizontalBox* Header = Make<UHorizontalBox>(Tree, TEXT("HBox_Skill") + N);

			UTextBlock* Title = MakeText(TEXT("Text_Skill") + N + TEXT("Title"), ResolveText(TEXT("Text_Skill") + N + TEXT("Title"), Row.Title, true),
				BodyFont, Row.TitleSize, SkillTitleColor);
			AddToHBox(Header, Title, 0.0f);

			if (Row.Key)
			{
				UTexture2D* KeyTexture = LoadObject<UTexture2D>(nullptr, Row.Key->Path);
				if (!KeyTexture) Log.Error(FString::Printf(TEXT("조작 버튼 그림 없음: %s"), Row.Key->Path));
				UImage* Key = Make<UImage>(Tree, TEXT("Image_Skill") + N + TEXT("Key"));
				Key->SetBrush(MakeImageBrush(KeyTexture, Row.Key->Size));
				AddToHBox(Header, Key, 10.0f);
			}

			if (Row.Cost.OldName || Row.Cost.Korean)
			{
				// 물 사용량 칸 — CharacterInfo-Water(133×38)를 예전 그림의 칸 높이 26 에 맞춘다(91×26).
				// 물방울은 그림의 x 33~43 → 칸에서 23~29, 숫자는 그 뒤 37 부터
				UTexture2D* WaterTexture = LoadObject<UTexture2D>(nullptr, WaterCostPath);
				if (!WaterTexture) Log.Error(FString::Printf(TEXT("물 사용량 그림 없음: %s"), WaterCostPath));
				USizeBox* Cost = Make<USizeBox>(Tree, TEXT("Cost_Skill") + N);
				Cost->SetWidthOverride(WaterCostSize.X);
				Cost->SetHeightOverride(WaterCostSize.Y);
				UOverlay* CostOverlay = Make<UOverlay>(Tree, TEXT("Overlay_Skill") + N + TEXT("Cost"));
				Cost->AddChild(CostOverlay);

				UImage* CostBg = Make<UImage>(Tree, TEXT("Image_Skill") + N + TEXT("CostBg"));
				CostBg->SetBrush(MakeImageBrush(WaterTexture, WaterCostSize));
				UOverlaySlot* BgSlot = CostOverlay->AddChildToOverlay(CostBg);
				BgSlot->SetHorizontalAlignment(HAlign_Fill);
				BgSlot->SetVerticalAlignment(VAlign_Fill);

				UTextBlock* CostText = MakeText(TEXT("Text_Skill") + N + TEXT("Cost"), ResolveText(TEXT("Text_Skill") + N + TEXT("Cost"), Row.Cost, false),
					NumberFont, 10.5f, NumberColor);
				UOverlaySlot* TextSlot = CostOverlay->AddChildToOverlay(CostText);
				TextSlot->SetHorizontalAlignment(HAlign_Left);
				TextSlot->SetVerticalAlignment(VAlign_Center);
				TextSlot->SetPadding(FMargin(37.0f, 0.0f, 0.0f, 0.0f));
				AddToHBox(Header, Cost, 26.0f);
			}
			return Header;
		}

		void Build(const FCharacterSpec& Spec)
		{
			// 다시 짓기 전에 글자를 모은다
			TArray<UWidget*> Existing;
			Tree->GetAllWidgets(Existing);
			for (UWidget* Widget : Existing)
			{
				if (const UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
				{
					OldTexts.Add(TextBlock->GetFName(), TextBlock->GetText());
				}
				else if (const URichTextBlock* RichText = Cast<URichTextBlock>(Widget))
				{
					OldTexts.Add(RichText->GetFName(), RichText->GetText());
				}
			}

			// 트리를 비운다 (루트 밖에 떠 있던 위젯까지)
			DiscardRecursive(Tree, Tree->RootWidget);
			Tree->RootWidget = nullptr;
			TArray<UWidget*> Leftovers;
			Tree->GetAllWidgets(Leftovers);
			for (UWidget* Leftover : Leftovers)
			{
				DiscardRecursive(Tree, Leftover);
			}

			UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CanvasPanel_Root"));
			Root->SetVisibility(ESlateVisibility::HitTestInvisible);
			Tree->RootWidget = Root;

			// 그림 크기 그대로 화면 가운데
			UCanvasPanel* Panel = Make<UCanvasPanel>(Tree, TEXT("CanvasPanel_Info"));
			UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetPosition(FVector2D::ZeroVector);
			PanelSlot->SetSize(ImageSize);

			UTexture2D* Background = LoadObject<UTexture2D>(nullptr, Spec.BackgroundPath);
			if (!Background) Log.Error(FString::Printf(TEXT("배경 그림 없음: %s"), Spec.BackgroundPath));
			UImage* BackgroundImage = Make<UImage>(Tree, TEXT("Image_Background"));
			BackgroundImage->SetBrush(MakeImageBrush(Background, ImageSize));
			UCanvasPanelSlot* BackgroundSlot = Panel->AddChildToCanvas(BackgroundImage);
			BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			BackgroundSlot->SetOffsets(FMargin(0.0f));

			// 캐릭터 이름 — 예전 그림의 글자 가운데 높이(새 좌표 y 183)에 맞춘다 (글자 칸 높이 80 의 대문자 가운데 ≈ 37)
			AddToCanvas(Panel, MakeTitle(Spec), FVector2D(248.0f, 146.0f), FVector2D::ZeroVector, true);

			// 체력 · 물 (오른쪽 위 칸)
			AddToCanvas(Panel, MakeText(TEXT("Text_Health"), ResolveText(TEXT("Text_Health"), Spec.Health, false), NumberFont, 13.5f, NumberColor),
				FromOld(1229.0f, 39.0f), FVector2D(62.0f, 22.0f), false);
			AddToCanvas(Panel, MakeText(TEXT("Text_Water"), ResolveText(TEXT("Text_Water"), Spec.Water, false), NumberFont, 13.5f, NumberColor),
				FromOld(1381.0f, 39.0f), FVector2D(52.0f, 22.0f), false);

			// 부제 — 번역이 길면 두 줄로
			UWidget* Subtitle = MakeWrappedText(TEXT("Text_Subtitle"), ResolveText(TEXT("Text_Subtitle"), Spec.Subtitle, true), 19.5f, 880.0f);
			AddToCanvas(Panel, Subtitle, FromOld(140.0f, 160.0f), FVector2D::ZeroVector, true);

			// 스킬 3줄 — 이름 줄(버튼 포함)과 설명. 설명은 캐릭터 그림 앞(560)에서 줄바꿈
			for (int32 Index = 0; Index < 3; ++Index)
			{
				const FSkillRowSpec& Row = Spec.Rows[Index];
				const FString N = FString::FromInt(Index + 1);
				AddToCanvas(Panel, MakeSkillHeader(Index + 1, Row), FromOld(262.0f, Row.TitleY), FVector2D::ZeroVector, true);
				AddToCanvas(Panel, MakeRichDesc(TEXT("Text_Skill") + N + TEXT("Desc"), ResolveText(TEXT("Text_Skill") + N + TEXT("Desc"), Row.Desc, true), 11.25f, 560.0f),
					FromOld(262.0f, Row.DescY), FVector2D::ZeroVector, true);
			}

			// 조작 난이도 — 오른쪽 아래 선 바로 앞에서 끝나게 (오른쪽 정렬)
			UTextBlock* Difficulty = MakeText(TEXT("Text_Difficulty"), ResolveText(TEXT("Text_Difficulty"), Spec.Difficulty, true), BodyFont, 12.0f, BodyColor);
			AddToCanvas(Panel, Difficulty, FromOld(1180.0f, 743.0f), FVector2D::ZeroVector, true, FVector2D(1.0f, 0.5f));
		}
	};

	// 설명 속 조작 버튼 그림 — <img id="LMB"/> 등. 표(DT) 줄을 다시 채우고 데코레이터 BP 의 ImageSet 에 꽂는다.
	// 두 에셋은 미리 있어야 한다(Python 이 만든다). 저장은 부른 쪽이 한다
	const TCHAR* KeyImageTablePath = TEXT("/Game/Blueprints/UI/CharacterInfo/DT_CharacterInfoKeyImages.DT_CharacterInfoKeyImages");
	const TCHAR* KeyDecoratorPath = TEXT("/Game/Blueprints/UI/CharacterInfo/BP_CharacterInfoKeyDecorator.BP_CharacterInfoKeyDecorator");

	TSubclassOf<URichTextBlockDecorator> PrepareKeyDecorator(FLog& Log)
	{
		UDataTable* Table = LoadObject<UDataTable>(nullptr, KeyImageTablePath);
		UBlueprint* DecoratorBlueprint = LoadObject<UBlueprint>(nullptr, KeyDecoratorPath);
		if (!Table || Table->GetRowStruct() != FRichImageRow::StaticStruct() || !DecoratorBlueprint || !DecoratorBlueprint->GeneratedClass
			|| !DecoratorBlueprint->GeneratedClass->IsChildOf(URichTextBlockImageDecorator::StaticClass()))
		{
			Log.Error(TEXT("조작 버튼 표/데코레이터 없음 (DT_CharacterInfoKeyImages · BP_CharacterInfoKeyDecorator)"));
			return nullptr;
		}

		const TPair<const TCHAR*, const FKeyIcon*> Rows[] = { { TEXT("LMB"), &KeyLMB }, { TEXT("RMB"), &KeyRMB }, { TEXT("E"), &KeyE }, { TEXT("Q"), &KeyQ } };
		Table->Modify();
		Table->EmptyTable();
		for (const TPair<const TCHAR*, const FKeyIcon*>& Row : Rows)
		{
			FRichImageRow ImageRow;
			ImageRow.Brush = MakeImageBrush(LoadObject<UTexture2D>(nullptr, Row.Value->Path), Row.Value->Size);
			Table->AddRow(FName(Row.Key), ImageRow);
		}
		Table->MarkPackageDirty();

		// ImageSet 은 protected 라 반사로 넣는다 (디테일 패널에서 고르는 것과 같다)
		UObject* DecoratorDefaults = DecoratorBlueprint->GeneratedClass->GetDefaultObject();
		if (FObjectPropertyBase* ImageSetProperty = FindFProperty<FObjectPropertyBase>(URichTextBlockImageDecorator::StaticClass(), TEXT("ImageSet")))
		{
			DecoratorDefaults->Modify();
			ImageSetProperty->SetObjectPropertyValue_InContainer(DecoratorDefaults, Table);
			// ★다시 컴파일해야 한다★ — BP 클래스는 컴파일 때 정한 목록(CDO 와 다른 값)만 새 인스턴스에 복사한다.
			// 컴파일 없이 CDO 만 고치면 RichText 가 만드는 데코레이터의 ImageSet 이 None 이라 <img> 가 글자 그대로 나온다
			FKismetEditorUtilities::CompileBlueprint(DecoratorBlueprint);
			DecoratorBlueprint->MarkPackageDirty();
		}
		return DecoratorBlueprint->GeneratedClass.Get();
	}


}
#endif

FString UDRCharacterInfoEditorTools::BuildCharacterInfoWidget(const FString& CharacterName)
{
#if WITH_EDITOR
	using namespace DRCharacterInfoEditor;
	FString Body;
	FLog Log{ Body };

	const FCharacterSpec* Spec = nullptr;
	for (const FCharacterSpec& Candidate : Specs)
	{
		if (CharacterName == Candidate.Name) Spec = &Candidate;
	}
	if (!Spec)
	{
		return FString::Printf(TEXT("ERROR 모르는 캐릭터: %s (Gardener / VendingMachine / RobotVacuum / BellBoy)\n"), *CharacterName);
	}

	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, Spec->WidgetPath);
	if (!Blueprint || !Blueprint->WidgetTree)
	{
		return FString::Printf(TEXT("ERROR WBP 없음: %s\n"), Spec->WidgetPath);
	}

	Blueprint->Modify();
	Blueprint->WidgetTree->Modify();

	FBuilder Builder{ Blueprint, Blueprint->WidgetTree, Log };
	Builder.TitleFont = LoadObject<UObject>(nullptr, TitleFontPath);
	Builder.BodyFont = LoadObject<UObject>(nullptr, BodyFontPath);
	Builder.NumberFont = LoadObject<UObject>(nullptr, NumberFontPath);
	if (!Builder.TitleFont || !Builder.BodyFont || !Builder.NumberFont)
	{
		return TEXT("ERROR 폰트 없음\n");
	}
	Builder.KeyDecorator = PrepareKeyDecorator(Log);
	Builder.Build(*Spec);

	const bool bCompiled = CompileAndMark(Blueprint, Log);
	return FString((bCompiled && !Log.bError) ? TEXT("OK ") : TEXT("ERROR ")) + Blueprint->GetName() + TEXT("\n") + Body;
#else
	return TEXT("ERROR 에디터 전용\n");
#endif
}
