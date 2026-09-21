// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "Game/DRCosmeticTypes.h"
#include "DRWardrobeTabWidget.generated.h"

/**
 * 옷장 화면의 카테고리 탭 1개(HEAD/FACE/BODY/TAIL)의 C++ 베이스. (Plan.md 15.6 참조)
 *
 * 디자이너는 WBP_Wardrobe 에 이 위젯을 4개 배치하고 Category 만 서로 다르게 지정한다.
 * 선택 표시는 소속 화면(UDRWardrobeScreenWidget)이 일괄로 밀어 넣으므로
 * BP 쪽에서 탭끼리 상태를 주고받을 필요가 없다.
 */
UCLASS(Abstract)
class DAERUNE_API UDRWardrobeTabWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	// 이 탭이 담당하는 카테고리. ★디자이너가 탭마다 다르게 지정한다★
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic", meta = (ExposeOnSpawn = "true"))
	EDRCosmeticCategory Category = EDRCosmeticCategory::Head;

	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	bool IsSelected() const { return bSelected; }

	// 소속 화면이 호출한다. 상태가 바뀔 때만 BP 이벤트를 발행한다.
	void SetSelected(bool bInSelected);

	// 탭 버튼 OnClicked 에서 부른다 → 소속 화면의 카테고리를 전환한다
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void HandleClicked();

protected:
	virtual void NativeConstruct() override;

	// 선택 상태가 바뀐 직후. BP 가 필(Img_Pill) 가시성과 라벨 색을 여기서 반영한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
	void OnSelectionChanged(bool bInSelected);

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	bool bSelected = false;
};
