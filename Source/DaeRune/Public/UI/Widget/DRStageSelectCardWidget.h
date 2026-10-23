// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "Game/DRStageSelectTypes.h"
#include "DRStageSelectCardWidget.generated.h"

class UDRStageSelectScreenWidget;
class UBorder;
class UImage;
class UTextBlock;
class UWidget;

/**
 * 스테이지 선택 화면의 카드 1장 C++ 베이스. (시안: 스테이지 선택.html 의 .card)
 *
 * ★이미지 없이도 동작하도록 그리기와 입력을 전부 C++ 이 한다★ — 아래 이름의 위젯이 WBP 에 있으면 자동 연결된다.
 * 전부 Optional 이라 일부만 만들어도 컴파일 · 실행된다.
 *   Border_Frame    [Border]  카드 테두리. 상태별 색 (기본 / 강조 / 잠김 / 잠김+호버 경고)
 *   Image_Thumbnail [Image]   배경 이미지. 뷰모델에 이미지가 있을 때만 브러시를 바꾼다. 잠기면 어둡게
 *   Txt_Number      [Text]    큰 번호 "01"
 *   Txt_Badge       [Text]    우상단 배지 "진행 가능" / "CLEAR" / "LOCKED"
 *   Border_Badge    [Border]  배지 테두리. ★브러시 외곽선 색★ 을 배지 글자색과 같이 바꾼다 (RoundedBox 외곽선일 때)
 *   Txt_Title       [Text]    "스테이지 1 · 연구 격납고"
 *   Txt_Mission     [Text]    임무 설명
 *   Txt_Done        [Text]    "임무 완료 · 다시 도전 가능" — 클리어일 때만 보인다
 *   Panel_Cleared   [아무 위젯] "CLEARED" 도장 — 클리어일 때만 보인다
 *   Panel_Locked    [아무 위젯] 자물쇠 + 어두운 덮개 — 잠겼을 때만 보인다
 *   Panel_Selected  [아무 위젯] 선택 표시(배경 밝게 등) — 선택됐을 때만 보인다
 *
 * 입력: 마우스 호버 / 클릭(선택) / 더블클릭(선택 + 시작). 처리는 소속 화면에 넘긴다.
 * 연출: 강조 시 살짝 떠오름(-6px, 1.015배), 잠긴 카드 클릭 시 좌우 흔들림 — 기본값이 C++ 에 있고 BP 이벤트로 덧붙일 수 있다.
 *
 * ★카드 안에 Button 을 두지 말 것★ — Button 이 마우스를 먼저 먹어 더블클릭/호버가 카드까지 오지 않는다.
 */
UCLASS(Abstract)
class DAERUNE_API UDRStageSelectCardWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	// 소속 화면이 카드를 만들 때 호출한다. ★카드에 데이터를 넣는 유일한 입구★
	void InitCard(UDRStageSelectScreenWidget* InOwnerScreen, const FDRStageSelectViewModel& InViewModel);

	UFUNCTION(BlueprintPure, Category = "StageSelect")
	const FDRStageSelectViewModel& GetViewModel() const { return ViewModel; }

	// 화면이 선택/키보드 포커스 표시를 밀어 넣는다
	void SetSelected(bool bInSelected);
	void SetKeyboardFocused(bool bInFocused);

	UFUNCTION(BlueprintPure, Category = "StageSelect")
	bool IsSelected() const { return bSelected; }

	// 마우스 호버 또는 키보드 포커스 — 시안의 :hover / :focus-visible
	UFUNCTION(BlueprintPure, Category = "StageSelect")
	bool IsHighlighted() const { return bHovered || bKeyboardFocused; }

	// 잠긴 카드를 고르려 했을 때 화면이 호출한다 (흔들림 + BP 이벤트)
	void PlayRejectFeedback();

	// 상태 배지 문구 (진행 가능 / CLEAR / LOCKED)
	UFUNCTION(BlueprintPure, Category = "StageSelect")
	static FText GetStateLabel(EDRStageSelectState State);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 뷰모델이 들어온 직후. 그림은 C++ 이 이미 바꿨다 — BP 는 추가 연출만.
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnViewModelUpdated();

	// 선택 / 강조(호버·포커스)가 바뀐 직후 (BP: 사운드, 애니메이션)
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnSelectedChanged(bool bNewSelected);

	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnHighlightChanged(bool bNewHighlighted);

	// 잠긴 카드를 클릭했을 때 (BP: 경고 사운드 등. 흔들림은 C++ 이 이미 한다)
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnSelectRejected();

	// ===== 스타일 (시안 색상 그대로) =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	FLinearColor FrameColor = FLinearColor(FColor(0x4F, 0xB4, 0xE6));		// --line2

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	FLinearColor HighlightColor = FLinearColor(FColor(0x8F, 0xE3, 0xFF));	// --line

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	FLinearColor LockedColor = FLinearColor(FColor(0x6B, 0x7F, 0x99));		// --lock

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	FLinearColor WarningColor = FLinearColor(FColor(0xFF, 0x8A, 0x8A));		// --warn

	// 잠긴 카드 배경 틴트 (시안: saturate(.3) brightness(.7))
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	FLinearColor LockedThumbnailTint = FLinearColor(0.35f, 0.35f, 0.38f, 1.0f);

	// 강조 시 떠오르는 높이(px)와 배율. 잠긴 카드는 떠오르지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	float HighlightLift = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	float HighlightScale = 1.015f;

	// 떠오름 보간 속도 (클수록 빠름. 시안 transition .18s)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style", meta = (ClampMin = "1.0"))
	float HighlightInterpSpeed = 18.0f;

	// 흔들림 (시안 .shake: .35s, ±8px)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	float ShakeDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	float ShakeAmplitude = 8.0f;

	// false 면 떠오름/흔들림을 C++ 이 하지 않는다 (BP 애니메이션으로 대체할 때)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	bool bUseDefaultMotion = true;

	// ===== 디자이너 위젯 (이름이 같으면 자동 연결 — 없어도 컴파일된다) =====

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Frame;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Thumbnail;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Number;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Badge;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Badge;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Title;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Mission;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Done;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Cleared;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Locked;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Selected;

private:
	// 뷰모델 → 글자 · 이미지 · 패널 표시
	void ApplyViewModelVisuals();

	// 선택/강조/잠김 조합 → 테두리 · 배지 색
	void UpdateFrameVisuals();

	void SetHovered(bool bInHovered);

	TWeakObjectPtr<UDRStageSelectScreenWidget> OwnerScreen;

	UPROPERTY(Transient)
	FDRStageSelectViewModel ViewModel;

	bool bSelected = false;
	bool bHovered = false;
	bool bKeyboardFocused = false;

	// 떠오름 진행도 (0 = 기본, 1 = 완전히 떠오름)
	float HighlightAlpha = 0.0f;

	// 남은 흔들림 시간 (0 이하면 흔들리지 않는 중)
	float ShakeTimeRemaining = 0.0f;
};
