// Copyright DaeRune


#include "UI/Widget/DRCosmeticPreviewWidget.h"
#include "Actor/DRCosmeticPreviewStage.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "DaeRune/DRLogChannels.h"

void UDRCosmeticPreviewWidget::InitializePreview(EPlayerCharacterClass CharacterClass,
	const TArray<FName>& SkinIds)
{
	PreviewStage = ADRCosmeticPreviewStage::Find(this);

	if (!PreviewStage.IsValid())
	{
		// 레벨에 스테이지를 안 놓았다고 옷장 기능 전체가 죽으면 안 된다 —
		// 프리뷰 영역만 접고 칸 목록은 그대로 동작한다.
		UE_LOG(LogDR, Warning,
			TEXT("[Cosmetic] 레벨에 ADRCosmeticPreviewStage 가 없어 3D 프리뷰를 표시하지 않습니다."));
		OnPreviewUnavailable();
		return;
	}

	ADRCosmeticPreviewStage* Stage = PreviewStage.Get();

	Stage->SetPreviewClass(CharacterClass);
	Stage->SetPreviewSkins(SkinIds);
	Stage->SetCaptureActive(true);

	UTextureRenderTarget2D* RenderTarget = Stage->GetRenderTarget();
	if (!PreviewMaterial || !RenderTarget)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[Cosmetic] 프리뷰 머티리얼(%s) 또는 RenderTarget(%s) 이 없어 화면에 그릴 수 없습니다."),
			PreviewMaterial ? TEXT("O") : TEXT("X"),
			RenderTarget ? TEXT("O") : TEXT("X"));
		OnPreviewUnavailable();
		return;
	}

	PreviewMID = UMaterialInstanceDynamic::Create(PreviewMaterial, this);
	if (PreviewMID)
	{
		PreviewMID->SetTextureParameterValue(PreviewTextureParameter, RenderTarget);
		OnPreviewReady(PreviewMID);
	}
}

void UDRCosmeticPreviewWidget::UpdatePreviewSkins(const TArray<FName>& SkinIds)
{
	if (ADRCosmeticPreviewStage* Stage = PreviewStage.Get())
	{
		Stage->SetPreviewSkins(SkinIds);
	}
}

void UDRCosmeticPreviewWidget::ResetRotation()
{
	if (ADRCosmeticPreviewStage* Stage = PreviewStage.Get())
	{
		Stage->ResetPreviewYaw();
	}
}

void UDRCosmeticPreviewWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 드래그 회전을 받으려면 히트 테스트가 켜져 있어야 한다.
	// 디자이너가 SelfHitTestInvisible 로 두는 실수를 여기서 흡수한다.
	SetVisibility(ESlateVisibility::Visible);
}

void UDRCosmeticPreviewWidget::NativeDestruct()
{
	// 상시 캡처는 로비 프레임을 그냥 깎아먹는다 — 화면을 닫으면 반드시 끈다.
	if (ADRCosmeticPreviewStage* Stage = PreviewStage.Get())
	{
		Stage->SetCaptureActive(false);
	}

	bDragging = false;
	PreviewStage.Reset();
	PreviewMID = nullptr;

	Super::NativeDestruct();
}

FReply UDRCosmeticPreviewWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !PreviewStage.IsValid())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	bDragging = true;

	// 마우스를 캡처해 두면 커서가 위젯 밖으로 나가도 회전이 이어진다
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UDRCosmeticPreviewWidget::NativeOnMouseMove(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bDragging) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);

	ADRCosmeticPreviewStage* Stage = PreviewStage.Get();
	if (!Stage) return Super::NativeOnMouseMove(InGeometry, InMouseEvent);

	// 오른쪽으로 끌면 캐릭터가 오른쪽으로 도는 느낌이 되도록 부호를 뒤집는다
	Stage->AddPreviewYaw(-InMouseEvent.GetCursorDelta().X * YawPerPixel);

	return FReply::Handled();
}

FReply UDRCosmeticPreviewWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bDragging || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

void UDRCosmeticPreviewWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	// 화면 전환 등으로 캡처를 잃어도 드래그 상태가 남지 않게 한다
	bDragging = false;

	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
