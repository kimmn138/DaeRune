// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "Game/DRStageSelectTypes.h"
#include "DRStageSelectCardWidget.generated.h"

class UDRStageSelectScreenWidget;
class UImage;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UWidget;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 스테이지 선택 화면의 카드 1장 C++ 베이스. (아트: Figma "대룬 ui > 스테이지" · 파일 레이어가이드)
 *
 * 카드는 그림 레이어를 상태에 따라 바꿔 끼워 그린다. 그림은 스테이지마다 모양이 달라서 데이터(FDRStageCardArt)에서 온다.
 *   기본   : Base       + 그림 + Outline       + 번호(70%) + 글자
 *   선택   : Base       + 그림 + SelectBorder  + 번호      + 글자    + 떠오름(6px 위 · 1.015배)
 *   잠김   : LockedBase + 그림 + LockedOutline + 번호 · 글자(40%)   + 자물쇠   ← 그림 · 배지 · 글자에 잠금 효과(색상 블렌드)
 *   클리어 : 위 상태 + CLEARED 도장
 *
 * 아래 이름의 위젯이 WBP 에 있으면 자동 연결된다 (전부 Optional — WBP 는 DRStageSelectEditorTools 가 짓는다):
 *   SizeBox_Card    [SizeBox] 카드 모양 크기 (CardArt.CardSize)
 *   Image_Base      [Image]   베이스 / 잠금용 베이스 — 글로우 캔버스라 카드 왼쪽 위 (-50,-50) 에 놓는다
 *   Image_Thumbnail [Image]   스테이지 그림 — (-13,-1)
 *   Image_Frame     [Image]   외곽 / 선택 테두리 / 잠금용 외곽 — (-50,-50)
 *   Panel_Cleared   [아무 위젯] CLEARED 도장 — 클리어일 때만
 *   Panel_Content   [아무 위젯] 번호 + 글자 묶음 — CardArt.ContentOffset 만큼 옮기고, 잠기면 40%
 *   Image_Number    [Image]   큰 번호 그림
 *   Txt_Title       [Text]    스테이지 이름 ("연구 격납고")
 *   Image_Badge     [Image]   "스테이지 N" 배지 바탕
 *   Txt_Badge       [Text]    "스테이지 N"
 *   Txt_Mission     [Text]    임무 설명
 *   Panel_Locked    [아무 위젯] 자물쇠 — 잠겼을 때만
 *
 * 입력: 마우스 호버 / 클릭 / 더블클릭을 소속 화면에 넘긴다 (선택 규칙은 화면이 정한다).
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

	// 화면이 선택 표시를 밀어 넣는다 (선택 = 선택 테두리 + 번호 100% + 떠오름)
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "StageSelect")
	bool IsSelected() const { return bSelected; }

	// 잠긴 카드를 고르려 했을 때 화면이 호출한다 (흔들림 + BP 이벤트)
	void PlayRejectFeedback();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 뷰모델이 들어온 직후. 그림은 C++ 이 이미 바꿨다 — BP 는 추가 연출만.
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnViewModelUpdated();

	// 선택이 바뀐 직후 (BP: 사운드, 애니메이션)
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnSelectedChanged(bool bNewSelected);

	// 잠긴 카드를 클릭했을 때 (BP: 경고 사운드 등. 흔들림은 C++ 이 이미 한다)
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnSelectRejected();

	// ===== 스타일 (Figma 값) =====

	/**
	 * 그림 · 배지에 잠금 효과(Figma 블렌드 모드 "색상")를 거는 UI 머티리얼 — M_UI_StageSelect_LockBlend.
	 * 텍스처 파라미터 "Texture", 스칼라 "LockAmount"(0 = 원래 색, 1 = 잠금 효과). 비우면 텍스처를 그대로 그린다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	TObjectPtr<UMaterialInterface> LockBlendMaterial;

	// 큰 번호 그림의 화면 높이(px). 가로는 그림 비율대로 (Figma: 01 = 72x70, 02 = 96x70)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style", meta = (ClampMin = "1"))
	float NumberHeight = 70.0f;

	// 선택되지 않은 카드의 번호 불투명도 (레이어가이드 "숫자 이미지 - 오퍼시티 70%")
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style", meta = (ClampMin = "0", ClampMax = "1"))
	float NumberIdleOpacity = 0.7f;

	// 잠긴 카드의 번호 · 글자 묶음 불투명도 (레이어가이드 "오퍼시티 40")
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style", meta = (ClampMin = "0", ClampMax = "1"))
	float LockedContentOpacity = 0.4f;

	// 글자색 (Figma #DDF7FF)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	FLinearColor TextColor = FLinearColor(FColor(0xDD, 0xF7, 0xFF));

	// 잠긴 카드의 글자색 — #DDF7FF 에 잠금 효과(색상 블렌드 #1F242E)를 건 색
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	FLinearColor LockedTextColor = FLinearColor(FColor(0xEC, 0xF1, 0xFB));

	// 선택된 카드가 떠오르는 높이(px)와 배율 (Figma 선택 카드: 6px 위 · 1.015배)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	float SelectedLift = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	float SelectedScale = 1.015f;

	// 떠오름 보간 속도 (클수록 빠름)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style", meta = (ClampMin = "1.0"))
	float LiftInterpSpeed = 18.0f;

	// 잠긴 카드를 눌렀을 때의 흔들림 (.35초, ±8px)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	float ShakeDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	float ShakeAmplitude = 8.0f;

	// false 면 떠오름/흔들림을 C++ 이 하지 않는다 (BP 애니메이션으로 대체할 때)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	bool bUseDefaultMotion = true;

	// ===== 디자이너 위젯 (이름이 같으면 자동 연결 — 없어도 컴파일된다) =====

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> SizeBox_Card;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Base;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Thumbnail;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Frame;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Cleared;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Content;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Number;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Title;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Badge;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Badge;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Mission;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Locked;

private:
	// 스테이지가 정해지면 한 번 — 크기 · 글자 · 그림 · 번호 · 배지
	void ApplyStageVisuals();

	// 선택 · 잠김 · 클리어에 따라 바뀌는 것 — 베이스/테두리 교체, 불투명도, 잠금 효과, 도장, 자물쇠
	void ApplyStateVisuals();

	// 텍스처를 원래 크기(또는 DrawSize)로 그린다. 텍스처가 없으면 숨긴다.
	static void SetImageTexture(UImage* Image, UTexture2D* Texture, FVector2D DrawSize = FVector2D::ZeroVector);

	// 잠금 효과 머티리얼로 그린다 (머티리얼이 없으면 텍스처 그대로). Mid 는 처음 한 번 만든다.
	void SetLockBlendImage(UImage* Image, TObjectPtr<UMaterialInstanceDynamic>& Mid, UTexture2D* Texture);

	TWeakObjectPtr<UDRStageSelectScreenWidget> OwnerScreen;

	UPROPERTY(Transient)
	FDRStageSelectViewModel ViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ThumbnailMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BadgeMID;

	bool bSelected = false;

	// 떠오름 진행도 (0 = 기본, 1 = 완전히 떠오름)
	float LiftAlpha = 0.0f;

	// 남은 흔들림 시간 (0 이하면 흔들리지 않는 중)
	float ShakeTimeRemaining = 0.0f;
};
