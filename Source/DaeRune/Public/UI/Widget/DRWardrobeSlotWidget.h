// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "Game/DRCosmeticTypes.h"
#include "DRWardrobeSlotWidget.generated.h"

class UImage;
class UButton;
class UTextBlock;
class UTexture2D;

/**
 * 옷장 화면의 칸 1개(216x252)의 C++ 베이스. (Plan(옷장) D-14)
 *
 * 뷰모델을 받아 ★그리기까지 C++ 이 한다★ — 아래 이름의 위젯이 WBP 에 있으면 자동 연결된다.
 *   Image_State      [Image]   상태 이미지 (기본 / 호버 / 선택 — StateXxxTexture)
 *   Image_Thumbnail  [Image]   썸네일. 아이콘이 없으면 숨긴다
 *   Txt_Name         [Text]    아이콘이 없을 때 대신 보이는 이름 (한글 → Pretendard)
 *   Image_Lock       [Image]   자물쇠. ★브러시가 지정돼 있을 때만★ 잠긴 칸에 보인다 (아트 미정 — D-18)
 *   Button_Slot      [Button]  투명 히트박스 (클릭 · 호버 · 툴팁)
 * OnViewModelUpdated 는 그 위에 연출을 더하고 싶을 때만 BP 에서 오버라이드한다.
 *
 * ★잠긴 칸도 버튼을 활성 상태로 둔다★ — 비활성 버튼은 UMG 에서 툴팁과 호버 이벤트를 받지 못해
 * "왜 잠겼는지" 안내가 뜨지 않는다. 클릭은 HandleClicked() 가 false 로 막는다.
 */
UCLASS(Abstract)
class DAERUNE_API UDRWardrobeSlotWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	// 소속 화면이 목록을 그릴 때 호출한다. ★칸에 데이터를 넣는 유일한 입구★ (Plan(옷장) T-20)
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void SetSkinViewModel(const FDRSkinViewModel& InViewModel);

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FDRSkinViewModel ViewModel;

	// 칸 버튼 OnClicked → 소속 화면에 장착 요청. 잠겨 있으면 false. (Button_Slot 이 있으면 C++ 이 자동 연결)
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	bool HandleClicked();

	/**
	 * 썸네일을 표시할 수 있는가.
	 * 아이콘 에셋이 아직 없는 스킨이 정상 상태이므로(Plan(옷장) D-18),
	 * 이 값이 false 면 썸네일 대신 이름 글자를 보여준다.
	 */
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	bool HasThumbnail() const { return ViewModel.PreviewIcon != nullptr; }

	// 잠긴 칸의 썸네일 회색조 틴트. 해금 상태면 흰색(원색). (Plan(옷장) D-14)
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	FLinearColor GetThumbnailTint() const;

	// 툴팁 문구 — 해금이면 설명, 잠겼으면 해금 조건
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	FText GetTooltipText() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 뷰모델이 갱신된 직후. 그림은 C++ 이 이미 바꿨다 — BP 는 추가 연출만.
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
	void OnViewModelUpdated();

	// 잠긴 칸의 썸네일 틴트 (디자이너 조정용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
	FLinearColor LockedTint = FLinearColor(0.35f, 0.35f, 0.35f, 1.0f);

	// 잠긴 칸의 이름 글자 불투명도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic", meta = (ClampMin = "0", ClampMax = "1"))
	float LockedNameOpacity = 0.4f;

	// 상태 이미지 3장 (WBP 의 Class Defaults — 속성_1_기본 / 속성_1_호버 / 속성_1_선택)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Style")
	TObjectPtr<UTexture2D> StateNormalTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Style")
	TObjectPtr<UTexture2D> StateHoveredTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Style")
	TObjectPtr<UTexture2D> StateSelectedTexture;

	// ===== 디자이너 위젯 (이름이 같으면 자동 연결 — 없어도 컴파일된다) =====

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_State;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Thumbnail;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Name;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Lock;

	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Slot;

private:
	UFUNCTION()
	void HandleSlotButtonClicked();

	UFUNCTION()
	void HandleSlotButtonHovered();

	UFUNCTION()
	void HandleSlotButtonUnhovered();

	// 뷰모델 → 위젯 전부 (상태 이미지 · 썸네일/이름 · 자물쇠 · 툴팁)
	void ApplyViewModelVisuals();

	// 선택 > 호버 > 기본 순으로 상태 이미지를 고른다
	void UpdateStateImage();

	bool bHovering = false;
};
