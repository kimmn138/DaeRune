// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "DRHealthWaterBarWidget.generated.h"

class UImage;
class UTexture2D;
class UOverlayWidgetController;

/**
 * 체력/물 HUD(WBP_HealthWaterBar)의 C++ 베이스.
 *
 * 장착 칩이 최대 물을 늘리면(확장 물탱크) 물 테두리 Image_Water 를 ExpandedWaterFrame(Water_150)으로 바꾸고,
 * 칩을 빼면 디자이너에 넣어 둔 원래 테두리로 되돌린다.
 * 신호는 UOverlayWidgetController::OnWaterTankExpandedChanged — 나머지 체력/물 그래프는 BP 그대로다.
 */
UCLASS()
class DAERUNE_API UDRHealthWaterBarWidget : public UDRUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeWidgetControllerSet() override;

	// 물 테두리. 이름이 같으면 디자이너의 이미지가 묶인다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Water")
	TObjectPtr<UImage> Image_Water;

	// 최대 물이 늘었을 때의 테두리 (WBP 기본값에서 Water_150 지정). 비어 있으면 바꾸지 않는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water")
	TObjectPtr<UTexture2D> ExpandedWaterFrame;

private:
	UFUNCTION()
	void HandleWaterTankExpandedChanged(bool bExpanded);

	void UnbindController();

	// 디자이너에 넣어 둔 원래 테두리 (칩을 뺐을 때 되돌릴 값)
	UPROPERTY(Transient)
	TObjectPtr<UObject> DefaultWaterFrame;

	TWeakObjectPtr<UOverlayWidgetController> BoundController;
};
