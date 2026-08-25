// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "Game/DRProgressionTypes.h"
#include "DRUpgradeSlotWidget.generated.h"

/**
 * 업그레이드 화면 좌측 슬롯 1칸의 C++ 베이스. (Plan2.md 21.7 참조)
 *
 * 뷰모델을 받아 BP 이벤트로 넘기는 것만 한다. 그리기는 WBP_UpgradeSlot 이 담당한다.
 * 슬롯은 종류 구분이 없으므로 이 위젯은 "어떤 칩이든 받을 수 있는 칸"으로만 동작한다.
 */
UCLASS(Abstract)
class DAERUNE_API UDRUpgradeSlotWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void SetSlotViewModel(const FDRSlotViewModel& InViewModel);

	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	FDRSlotViewModel ViewModel;

	// 드래그 소스가 될 수 있는 칸인가 (점유된 칸만 끌어낼 수 있다)
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	bool IsDraggable() const { return ViewModel.State == EDRSlotState::Occupied; }

protected:
	// 뷰모델이 갱신된 직후. BP 가 텍스처/텍스트를 여기서 반영한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Upgrade")
	void OnViewModelUpdated();
};
