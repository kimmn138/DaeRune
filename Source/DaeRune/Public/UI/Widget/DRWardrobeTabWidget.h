// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "Game/DRCosmeticTypes.h"
#include "Styling/SlateColor.h"
#include "DRWardrobeTabWidget.generated.h"

class UImage;
class UButton;
class UTextBlock;

/**
 * 옷장 화면의 카테고리 탭 1개(HEAD/FACE/BODY/TAIL)의 C++ 베이스. (Plan(옷장) D-15)
 *
 * 디자이너는 WBP_Wardrobe 에 이 위젯을 4개 배치하고 Category · LabelText 만 서로 다르게 지정한다.
 * 선택 표시는 소속 화면(UDRWardrobeScreenWidget)이 일괄로 밀어 넣으므로
 * BP 쪽에서 탭끼리 상태를 주고받을 필요가 없다.
 *
 * ★그리기도 C++ 이 한다★ — 아래 이름의 위젯이 WBP 에 있으면 자동으로 연결되어
 * 클릭 → 카테고리 전환, 선택 필 표시, 라벨 글자/색이 BP 배선 없이 동작한다.
 *   Image_Pill  [Image]   선택됐을 때만 보이는 필 (카테고리_선택박스)
 *   Button_Tab  [Button]  투명 히트박스
 *   Text_Label  [Text]    라벨
 * OnSelectionChanged 는 그 위에 연출을 더하고 싶을 때만 BP 에서 오버라이드한다.
 */
UCLASS(Abstract)
class DAERUNE_API UDRWardrobeTabWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	// 이 탭이 담당하는 카테고리. ★디자이너가 탭마다 다르게 지정한다★ (Plan(옷장) T-21)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic", meta = (ExposeOnSpawn = "true"))
	EDRCosmeticCategory Category = EDRCosmeticCategory::Head;

	// 탭에 표시할 글자. ★대문자로 입력한다★ — Bruno Ace SC 는 소문자를 작은 대문자로 그린다 (Plan(옷장) T-19)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic", meta = (ExposeOnSpawn = "true"))
	FText LabelText;

	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	bool IsSelected() const { return bSelected; }

	// 소속 화면이 호출한다. 상태가 바뀔 때만 그림을 바꾸고 BP 이벤트를 발행한다.
	void SetSelected(bool bInSelected);

	// 탭 버튼 OnClicked → 소속 화면의 카테고리를 전환한다 (Button_Tab 이 있으면 C++ 이 자동 연결)
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void HandleClicked();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 선택 상태가 바뀐 직후. 필 · 라벨 색은 C++ 이 이미 바꿨다 — BP 는 추가 연출만.
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
	void OnSelectionChanged(bool bInSelected);

	// 라벨 색. 기본값 = 2026-10-19 시안 실측 (sRGB DDF7FF / 6676B8 — Plan(옷장) D-24)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic|Style")
	FSlateColor SelectedLabelColor = FSlateColor(FLinearColor(0.723f, 0.930f, 1.0f, 1.0f));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic|Style")
	FSlateColor UnselectedLabelColor = FSlateColor(FLinearColor(0.133f, 0.181f, 0.479f, 1.0f));

	// ===== 디자이너 위젯 (이름이 같으면 자동 연결 — 없어도 컴파일된다) =====

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Pill;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Tab;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Label;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	bool bSelected = false;

private:
	// 필 표시 + 라벨 색을 현재 선택 상태에 맞춘다
	void ApplySelectionVisuals();
};
