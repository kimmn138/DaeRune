// Copyright DaeRune

#include "UI/Widget/Stage2/DRS2PuzzleTileWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Curves/CurveFloat.h"

FVector2D UDRS2PuzzleTileWidget::IndexToPosition(int32 Index)
{
	const int32 Col = Index % DRS2Puzzle::GridSize;
	const int32 Row = Index / DRS2Puzzle::GridSize;
	return FVector2D(Col * DRS2Puzzle::CellPitch, Row * DRS2Puzzle::CellPitch);
}

EDRS2SlideDir UDRS2PuzzleTileWidget::ComputeDirection(int32 From, int32 To)
{
	const int32 FromRow = From / DRS2Puzzle::GridSize;
	const int32 FromCol = From % DRS2Puzzle::GridSize;
	const int32 ToRow   = To   / DRS2Puzzle::GridSize;
	const int32 ToCol   = To   % DRS2Puzzle::GridSize;

	if (FromRow == ToRow)
	{
		if (ToCol == FromCol + 1) { return EDRS2SlideDir::Right; }
		if (ToCol == FromCol - 1) { return EDRS2SlideDir::Left; }
	}
	else if (FromCol == ToCol)
	{
		if (ToRow == FromRow + 1) { return EDRS2SlideDir::Down; }
		if (ToRow == FromRow - 1) { return EDRS2SlideDir::Up; }
	}
	return EDRS2SlideDir::None;
}

void UDRS2PuzzleTileWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// ★AddUniqueDynamic 이어야 한다★ - 위젯을 뷰포트에서 뺐다가 다시 넣으면
	// NativeConstruct 가 한 번 더 돌아 AddDynamic 은 같은 핸들러를 두 번 등록한다.
	// 그러면 클릭 한 번에 이동 요청이 두 번 날아가 조각이 두 칸 뛴다.
	if (Button_Tile)
	{
		Button_Tile->OnClicked.AddUniqueDynamic(this, &UDRS2PuzzleTileWidget::HandleClicked);
		Button_Tile->OnHovered.AddUniqueDynamic(this, &UDRS2PuzzleTileWidget::HandleHovered);
		Button_Tile->OnUnhovered.AddUniqueDynamic(this, &UDRS2PuzzleTileWidget::HandleUnhovered);
	}

	if (Img_Hover)
	{
		Img_Hover->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UDRS2PuzzleTileWidget::NativeDestruct()
{
	StopMotion();

	OnTileClicked.Unbind();
	OnSlideFinished.Unbind();

	Super::NativeDestruct();
}

void UDRS2PuzzleTileWidget::InitTile(int32 InTileId, UTexture2D* InTexture)
{
	TileId = InTileId;

	if (Img_Tile && InTexture)
	{
		Img_Tile->SetBrushFromTexture(InTexture, false);

		// ★bMatchSize=false 로 붙였으므로 크기를 직접 못 박는다★
		// 안 그러면 브러시가 텍스처 원본 크기를 Desired Size 로 들고 와서 칸을 벗어난다.
		Img_Tile->SetDesiredSizeOverride(FVector2D(DRS2Puzzle::CellSize, DRS2Puzzle::CellSize));
	}
}

void UDRS2PuzzleTileWidget::SnapToIndex(int32 InIndex)
{
	GridIndex = InIndex;

	Motion       = EDRS2TileMotion::Idle;
	SlideElapsed = 0.f;
	ShakeElapsed = 0.f;

	if (UWidget* Target = GetMotionTarget())
	{
		Target->SetRenderTranslation(FVector2D::ZeroVector);
	}

	SlideEndPos = IndexToPosition(InIndex);
	SetSlotPosition(SlideEndPos);

	// 즉시 배치는 '이동'이 아니므로 완료 델리게이트를 쏘지 않는다.
	// (쏘면 보드의 PendingSlides 카운터가 음수로 내려가 입력 잠금이 꼬인다)
}

void UDRS2PuzzleTileWidget::SlideToIndex(int32 InIndex, float Duration)
{
	// 방향 연출용으로 덮어쓰기 전의 칸을 먼저 챙긴다.
	const int32 PrevIndex = GridIndex;

	GridIndex   = InIndex;
	SlideEndPos = IndexToPosition(InIndex);

	if (Duration <= KINDA_SMALL_NUMBER)
	{
		SnapToIndex(InIndex);
		OnSlideFinished.ExecuteIfBound(this);
		return;
	}

	// 흔들리던 중이었다면 오프셋을 먼저 지운다. 남겨 두면 이동 내내 그림이 옆으로 밀린다.
	if (UWidget* Target = GetMotionTarget())
	{
		Target->SetRenderTranslation(FVector2D::ZeroVector);
	}
	ShakeElapsed = 0.f;

	FVector2D CurrentPos = FVector2D::ZeroVector;
	if (!GetSlotPosition(CurrentPos))
	{
		// Canvas Slot 이 아직 없다면 보간할 좌표계가 없다. 즉시 배치로 대체한다.
		SnapToIndex(InIndex);
		OnSlideFinished.ExecuteIfBound(this);
		return;
	}

	SlideStartPos = CurrentPos;
	SlideDuration = Duration;
	SlideElapsed  = 0.f;
	Motion        = EDRS2TileMotion::Sliding;

	OnSlideStarted(ComputeDirection(PrevIndex, InIndex));
}

void UDRS2PuzzleTileWidget::PlayInvalidShake()
{
	// 이동 중이면 흔들지 않는다. 두 연출이 같은 프레임에 좌표를 다투면 그림이 튄다.
	if (Motion == EDRS2TileMotion::Sliding)
	{
		return;
	}

	OnInvalidMove();

	// 디자이너가 WBP 에 Anim_Invalid 를 만들어 두었으면 그쪽이 우선이다.
	if (Anim_Invalid)
	{
		Motion = EDRS2TileMotion::Idle;
		if (UWidget* Target = GetMotionTarget())
		{
			Target->SetRenderTranslation(FVector2D::ZeroVector);
		}
		PlayAnimation(Anim_Invalid);
		return;
	}

	// 없으면 코드로 흔든다. 이미 흔들리는 중이면 처음부터 다시 (연타에 반응이 남는다).
	Motion       = EDRS2TileMotion::Shaking;
	ShakeElapsed = 0.f;
}

void UDRS2PuzzleTileWidget::PlayPressFeedback()
{
	if (Anim_Press)
	{
		PlayAnimation(Anim_Press);
	}
}

void UDRS2PuzzleTileWidget::StopMotion()
{
	Motion       = EDRS2TileMotion::Idle;
	SlideElapsed = 0.f;
	ShakeElapsed = 0.f;

	if (Anim_Invalid && IsAnimationPlaying(Anim_Invalid)) { StopAnimation(Anim_Invalid); }
	if (Anim_Press   && IsAnimationPlaying(Anim_Press))   { StopAnimation(Anim_Press); }

	if (UWidget* Target = GetMotionTarget())
	{
		Target->SetRenderTranslation(FVector2D::ZeroVector);
	}
}

void UDRS2PuzzleTileWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 멈춰 있는 조각은 여기서 끝난다. 8개가 매 프레임 도는 비용의 대부분을 이 한 줄이 막는다.
	if (Motion == EDRS2TileMotion::Idle)
	{
		return;
	}

	if (Motion == EDRS2TileMotion::Sliding)
	{
		TickSlide(InDeltaTime);
	}
	else
	{
		TickShake(InDeltaTime);
	}
}

void UDRS2PuzzleTileWidget::TickSlide(float DeltaTime)
{
	SlideElapsed += DeltaTime;

	const float Raw = FMath::Clamp(SlideElapsed / SlideDuration, 0.f, 1.f);
	const float Alpha = SlideCurve
		? SlideCurve->GetFloatValue(Raw)
		: FMath::InterpEaseOut(0.f, 1.f, Raw, 2.f);

	SetSlotPosition(FMath::Lerp(SlideStartPos, SlideEndPos, Alpha));

	if (Raw >= 1.f)
	{
		// 커브가 1.0 에서 정확히 끝나지 않는 경우가 있어 마지막에 목표 좌표를 못 박는다.
		SetSlotPosition(SlideEndPos);

		Motion       = EDRS2TileMotion::Idle;
		SlideElapsed = 0.f;

		OnSlideEnded();
		OnSlideFinished.ExecuteIfBound(this);
	}
}

void UDRS2PuzzleTileWidget::TickShake(float DeltaTime)
{
	ShakeElapsed += DeltaTime;

	const float U = FMath::Clamp(ShakeElapsed / ShakeDuration, 0.f, 1.f);

	// 감쇠 사인파: 진폭이 선형으로 줄어들며 좌우로 ShakeCycles 번 왕복한다.
	// U=0 과 U=1 에서 오프셋이 정확히 0 이라 시작/끝에 튐이 없다.
	const float Damping = 1.f - U;
	const float OffsetX = ShakeAmplitude * Damping * FMath::Sin(2.f * PI * ShakeCycles * U);

	if (UWidget* Target = GetMotionTarget())
	{
		Target->SetRenderTranslation(FVector2D(OffsetX, 0.f));
	}

	if (U >= 1.f)
	{
		if (UWidget* Target = GetMotionTarget())
		{
			Target->SetRenderTranslation(FVector2D::ZeroVector);
		}
		Motion       = EDRS2TileMotion::Idle;
		ShakeElapsed = 0.f;
	}
}

void UDRS2PuzzleTileWidget::HandleClicked()
{
	// 움직이는 중에는 클릭을 흘려보낸다. 보드도 따로 막지만, 여기서 한 번 더 거른다.
	if (Motion != EDRS2TileMotion::Idle)
	{
		return;
	}

	// ★눌림 연출을 여기서 재생하지 않는다★
	// 갈 수 있는지 아직 모르기 때문이다. 판정 후 보드가 PlayPressFeedback() 이나
	// PlayInvalidShake() 중 하나만 호출한다. 둘이 같은 Render Transform 을 다투지 않는다.
	OnTileClicked.ExecuteIfBound(TileId);
}

void UDRS2PuzzleTileWidget::HandleHovered()
{
	if (Img_Hover)
	{
		Img_Hover->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	OnHoverChanged(true);
}

void UDRS2PuzzleTileWidget::HandleUnhovered()
{
	if (Img_Hover)
	{
		Img_Hover->SetVisibility(ESlateVisibility::Hidden);
	}
	OnHoverChanged(false);
}

void UDRS2PuzzleTileWidget::SetSlotPosition(const FVector2D& InPosition)
{
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetPosition(InPosition);
	}
}

bool UDRS2PuzzleTileWidget::GetSlotPosition(FVector2D& OutPosition) const
{
	if (const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		OutPosition = CanvasSlot->GetPosition();
		return true;
	}
	return false;
}

UWidget* UDRS2PuzzleTileWidget::GetMotionTarget()
{
	return Overlay_Content ? static_cast<UWidget*>(Overlay_Content) : static_cast<UWidget*>(this);
}
