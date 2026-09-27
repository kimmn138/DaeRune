// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "Game/DRCosmeticTypes.h"
#include "DRWardrobeSlotWidget.generated.h"

/**
 * 옷장 화면의 칸 1개(216x252)의 C++ 베이스. (Plan.md 15.7 참조)
 *
 * 뷰모델을 받아 BP 이벤트로 넘기는 것만 한다. 그리기는 WBP_WardrobeSlot 이 담당한다.
 * (UDRUpgradeSlotWidget 과 같은 역할·같은 구조)
 *
 * ★잠긴 칸도 버튼을 활성 상태로 둔다★ — 비활성 버튼은 UMG 에서 툴팁과 호버 이벤트를 받지 못해
 * "왜 잠겼는지" 안내가 뜨지 않는다. 클릭은 HandleClicked() 가 false 로 막는다.
 */
UCLASS(Abstract)
class DAERUNE_API UDRWardrobeSlotWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	// 소속 화면이 목록을 그릴 때 호출한다
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void SetSkinViewModel(const FDRSkinViewModel& InViewModel);

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FDRSkinViewModel ViewModel;

	// 칸 버튼 OnClicked 에서 부른다 → 소속 화면에 장착 요청. 잠겨 있으면 false.
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	bool HandleClicked();

	/**
	 * 썸네일을 표시할 수 있는가.
	 * 아이콘 에셋이 아직 없는 스킨이 정상 상태이므로(Plan.md 15.12-7),
	 * BP 는 이 값으로 Img_Thumbnail 을 Visible/Collapsed 토글한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	bool HasThumbnail() const { return ViewModel.PreviewIcon != nullptr; }

	// 잠긴 칸의 썸네일 회색조 틴트. 해금 상태면 흰색(원색). (Plan.md 15.7)
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	FLinearColor GetThumbnailTint() const;

	// 툴팁 문구 — 해금이면 설명, 잠겼으면 해금 조건
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	FText GetTooltipText() const;

protected:
	// 뷰모델이 갱신된 직후. BP 가 버튼 스타일 브러시와 텍스처를 여기서 반영한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
	void OnViewModelUpdated();

	// 잠긴 칸의 썸네일 틴트 (디자이너 조정용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
	FLinearColor LockedTint = FLinearColor(0.35f, 0.35f, 0.35f, 1.0f);
};
