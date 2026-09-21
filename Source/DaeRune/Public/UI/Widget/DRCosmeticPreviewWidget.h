// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRCosmeticPreviewWidget.generated.h"

class ADRCosmeticPreviewStage;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 옷장 화면 안의 3D 캐릭터 프리뷰. (Plan.md 5.8.3 참조)
 *
 * ★UMG 에 3D 메시를 직접 넣을 수 없으므로★ 맵 밖 스튜디오(ADRCosmeticPreviewStage)가 찍은
 * RenderTarget 을 머티리얼로 감싸 Image 위젯에 물린다.
 * 보이는 것은 그림이 아니라 매 프레임 갱신되는 실제 캐릭터다.
 *
 * WBP 구성:
 *   WBP_CosmeticPreview (Parent: 이 클래스)
 *   └── Img_Preview  [Image]   Brush.Material = GetPreviewMaterial() (OnPreviewReady 에서 대입)
 *
 * ★루트/이미지의 Visibility 를 `Visible` 로 둘 것★ — `SelfHitTestInvisible` 이면
 * 드래그 회전 입력을 받지 못한다.
 */
UCLASS(Abstract)
class DAERUNE_API UDRCosmeticPreviewWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	// 스테이지를 찾아 캐릭터를 세우고 캡처를 켠다. 옷장 화면이 열릴 때 1회 호출된다.
	void InitializePreview(EPlayerCharacterClass CharacterClass, const TArray<FName>& SkinIds);

	// 장착이 바뀔 때마다 호출 — 서버 왕복 없이 그 자리에서 갈아입는다
	void UpdatePreviewSkins(const TArray<FName>& SkinIds);

	// Img_Preview 의 Brush 에 물릴 머티리얼. 스테이지가 없으면 nullptr.
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	UMaterialInstanceDynamic* GetPreviewMaterial() const { return PreviewMID; }

	// 프리뷰를 쓸 수 있는가 (레벨에 스테이지가 배치돼 있는가)
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	bool HasPreviewStage() const { return PreviewStage.IsValid(); }

	// 기본 각도로 되돌린다 (BP 버튼에서 부를 수 있게 노출)
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void ResetRotation();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	// 렌더 타깃을 넣을 머티리얼 (M_CosmeticPreview). BP 에서 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	// 위 머티리얼의 텍스처 파라미터 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
	FName PreviewTextureParameter = FName("Tex");

	// 가로 1px 드래그당 회전 각도. 부호를 뒤집으면 회전 방향이 바뀐다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
	float YawPerPixel = 0.5f;

	// 머티리얼 준비 완료 (BP: Img_Preview 의 Brush 에 물린다)
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
	void OnPreviewReady(UMaterialInstanceDynamic* Material);

	// 레벨에 스테이지가 없다 (BP: 프리뷰 영역 숨김 — Plan.md 15.12 폴백)
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
	void OnPreviewUnavailable();

private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PreviewMID;

	TWeakObjectPtr<ADRCosmeticPreviewStage> PreviewStage;

	bool bDragging = false;
};
