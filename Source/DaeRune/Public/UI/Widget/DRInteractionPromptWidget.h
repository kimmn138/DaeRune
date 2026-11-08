// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "Interaction/DRInteractable.h"
#include "DRInteractionPromptWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;
class UWidget;

/**
 * 화면 상호작용 안내 (시안 2026-11-08) — 조준선 아래 "[F] 부품 떨어뜨리기" 한 줄,
 * 부품을 들고 있으면 그 아래 부품 아이콘. 예전에는 대상 액터마다 월드 위젯(WidgetComponent)을 띄웠다.
 *
 * 무엇을 띄울지는 UDRInteractionComponent 가 정해 SetPrompt 로 넘긴다 — 이 위젯은 그리기만 한다.
 * WBP_InteractionPrompt 의 트리는 UDRInteractionPromptEditorTools 가 통째로 다시 짓는다 (손으로 고친 배치는 덮어써진다).
 *
 * ★그림은 WBP 의 클래스 기본값(Interaction Prompt|Icons)에 넣는다★ — KeyIcons(키별 그림) · CarriedPartIcon.
 * 비어 있으면 글자 키캡(Panel_KeyCap)과 빈 틀(Panel_PartPlaceholder)을 대신 그린다.
 */
UCLASS(Abstract)
class DAERUNE_API UDRInteractionPromptWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	// 안내를 바꾼다. 문구가 비면 안내 줄을, bShowCarriedPart 가 false 면 부품 아이콘을 숨긴다
	void SetPrompt(const FDRInteractionPrompt& Prompt);

	// 전부 숨긴다
	void HidePrompt() { SetPrompt(FDRInteractionPrompt()); }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 키별 그림 (예: F → F 키 아이콘). 없는 키는 글자 키캡으로 그린다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Prompt|Icons")
	TMap<FKey, TObjectPtr<UTexture2D>> KeyIcons;

	// 키 그림 높이 (가로는 그림 비율대로)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Prompt|Icons", meta = (ClampMin = "1"))
	float KeyIconHeight = 26.f;

	// 들고 있는 부품 아이콘. 비어 있으면 빈 틀을 그린다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Prompt|Icons")
	TObjectPtr<UTexture2D> CarriedPartIcon;

	// 부품 아이콘 높이 (가로는 그림 비율대로)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Prompt|Icons", meta = (ClampMin = "1"))
	float CarriedPartIconHeight = 74.f;

	// 안내 줄 · 부품 아이콘이 새로 뜰 때 서서히 나타나는 시간 (0 = 바로)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Prompt", meta = (ClampMin = "0"))
	float FadeInDuration = 0.12f;

	// 디자이너 미리보기에 그릴 키 (넣은 키 그림이 어떻게 보이는지 확인용)
	UPROPERTY(EditDefaultsOnly, Category = "Interaction Prompt|Preview")
	FKey PreviewKey = EKeys::F;

	// ===== 트리 (WBP_InteractionPrompt) =====

	// [키][문구] 한 줄
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> Panel_Prompt;

	// 키 자리 — 키 없는 안내면 통째로 접혀 문구가 가운데로 온다 (문구와의 간격도 이 슬롯의 여백이다)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> Panel_Key;

	// 키 그림 (KeyIcons 에 그 키가 있을 때)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Image_Key;

	// 글자 키캡 (그림이 없을 때) — 둥근 테두리 안에 키 이름
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> Panel_KeyCap;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Key;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Prompt;

	// 부품 아이콘 자리
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> Panel_CarriedPart;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Image_CarriedPart;

	// 부품 아이콘 그림이 없을 때의 빈 틀
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> Panel_PartPlaceholder;

private:
	void ApplyKey(const FKey& Key);
	void ApplyCarriedPartIcon();

	// 접혀 있던 패널을 보이며 서서히 나타나게 한다 (이미 보이던 것은 그대로)
	void ShowPanel(UWidget* Panel, float& FadeElapsed);
	static void HidePanel(UWidget* Panel, float& FadeElapsed);

	FDRInteractionPrompt CurrentPrompt;

	// 서서히 나타나는 중인 경과 시간 (음수 = 진행 중 아님)
	float PromptFadeElapsed = -1.f;
	float PartFadeElapsed = -1.f;
};
