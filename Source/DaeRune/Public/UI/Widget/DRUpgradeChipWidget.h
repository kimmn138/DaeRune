// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "Game/DRProgressionTypes.h"
#include "DRUpgradeChipWidget.generated.h"

/**
 * 업그레이드 화면 우측 칩 카드 1개의 C++ 베이스. (Plan2.md 21.7 참조)
 *
 * 뷰모델을 받아 BP 이벤트로 넘기는 것만 한다. 그리기는 WBP_UpgradeChip 이 담당한다.
 */
UCLASS(Abstract)
class DAERUNE_API UDRUpgradeChipWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void SetChipViewModel(const FDRChipViewModel& InViewModel);

	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	FDRChipViewModel ViewModel;

	/**
	 * 목록에서 끌어낼 수 있는 칩인가.
	 * - 미해금 칩은 못 든다 (해금 단계 미달)
	 * - 이미 장착된 칩은 못 든다 → 좌측 슬롯에서 빼거나 옮겨야 한다
	 */
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	bool IsDraggable() const { return ViewModel.bUnlocked && !ViewModel.bEquipped; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Upgrade")
	void OnViewModelUpdated();
};
