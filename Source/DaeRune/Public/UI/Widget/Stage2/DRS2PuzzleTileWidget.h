// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/Stage2/DRS2SlidePuzzleTypes.h"
#include "UI/Widget/DRUserWidget.h"
#include "DRS2PuzzleTileWidget.generated.h"

class UButton;
class UCurveFloat;
class UImage;
class UOverlay;
class UDRS2PuzzleTileWidget;

// 보드 기하 상수(namespace DRS2Puzzle)는 DRS2SlidePuzzleTypes.h 로 옮겼다 (2026-09-20).
// 서버 액터 ADRS2SlidePuzzle 이 같은 상수와 인접 판정을 써야 하는데,
// 액터가 UI 위젯 헤더를 include 하게 둘 수는 없기 때문이다.

/**
 * 조각이 지금 무엇을 하고 있는가. 셋 중 하나만 성립한다.
 *
 * ★이 상태값이 "이동 중 클릭 무시"의 근거다★ — Idle 이 아닌 조각은 클릭을 흘려보낸다.
 */
UENUM(BlueprintType)
enum class EDRS2TileMotion : uint8
{
	// 멈춰 있음. 클릭을 받을 수 있는 유일한 상태
	Idle,
	// 빈칸으로 미끄러지는 중 (Canvas Slot Position 보간)
	Sliding,
	// "그쪽으로 못 간다" 좌우 흔들림 중 (Render Translation 보간)
	Shaking
};

/** 연출 전용 방향값. 이동 가능 판정 자체는 항상 인덱스로만 한다. */
UENUM(BlueprintType)
enum class EDRS2SlideDir : uint8
{
	None,
	Up,
	Down,
	Left,
	Right
};

// 조각 -> 보드. 클릭된 조각 번호를 알린다 (판정은 보드가 한다)
DECLARE_DELEGATE_OneParam(FDRS2TileClicked, int32 /*TileId*/);
// 조각 -> 보드. 미끄러짐이 끝났다 (보드가 입력 잠금을 푸는 신호)
DECLARE_DELEGATE_OneParam(FDRS2TileSlideFinished, UDRS2PuzzleTileWidget* /*Tile*/);

/**
 * 8퍼즐 조각 1개. WBP_S2PuzzleTile 의 C++ 베이스.
 *
 * 이 클래스는 "내가 어디로 가야 하는지"를 스스로 판단하지 않는다.
 * 판정은 전부 UDRS2SlidePuzzleWidget 이 하고, 조각은 두 가지 움직임만 책임진다.
 *
 *   1) SlideToIndex()      - 지정된 칸으로 Duration 초 동안 미끄러진다
 *   2) PlayInvalidShake()  - 좌우로 짧게 흔들리고 제자리로 돌아온다
 *
 * ★두 움직임의 구현 채널이 다르다★
 *   - 미끄러짐은 Canvas Slot 의 Position 을 직접 옮긴다. 조각의 '진짜 자리'가 바뀌는 것이므로
 *     다음 이동의 출발점도 함께 바뀌어야 한다. Render Transform 으로 하면 실제 좌표는
 *     제자리에 남아 두 번째 이동부터 출발점이 어긋난다.
 *   - 흔들림은 Render Translation 을 쓴다. 자리는 그대로고 그림만 떨리는 것이므로
 *     레이아웃을 건드리면 안 된다. 끝나면 오프셋을 0 으로 되돌린다.
 *
 * WBP 가 준비해야 하는 것은 하이어라키뿐이다. 애니메이션 에셋은 없어도 동작한다
 * (Anim_Invalid / Anim_Press 는 BindWidgetAnimOptional 이라 만들어 두면 코드 연출을 대체한다).
 */
UCLASS(Abstract)
class DAERUNE_API UDRS2PuzzleTileWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	// ===== 보드가 바인딩하는 C++ 델리게이트 (BP 노출 없음: 내부 배선이다) =====
	FDRS2TileClicked       OnTileClicked;
	FDRS2TileSlideFinished OnSlideFinished;

	/** 조각 번호(1~8)와 그림을 주입한다. 보드가 생성 직후 1회 호출한다. */
	void InitTile(int32 InTileId, UTexture2D* InTexture);

	/** 애니메이션 없이 즉시 해당 칸으로. 셔플/초기 배치 전용 (완료 델리게이트를 쏘지 않는다). */
	void SnapToIndex(int32 InIndex);

	/** Duration 초에 걸쳐 해당 칸으로 미끄러진다. 완료 시 OnSlideFinished 발화. */
	void SlideToIndex(int32 InIndex, float Duration);

	/** 갈 수 없는 조각을 눌렀을 때의 좌우 흔들림. */
	void PlayInvalidShake();

	/** 정상 이동이 확정된 순간의 눌림 연출. 흔들림과 동시에 나오지 않도록 보드가 택일해 호출한다. */
	void PlayPressFeedback();

	/** 진행 중인 모든 움직임을 끊고 오프셋을 0 으로 되돌린다 (Reset / 위젯 정리용). */
	void StopMotion();

	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	int32 GetTileId() const { return TileId; }

	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	int32 GetGridIndex() const { return GridIndex; }

	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	EDRS2TileMotion GetMotion() const { return Motion; }

	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	bool IsIdle() const { return Motion == EDRS2TileMotion::Idle; }

	/** 칸 인덱스 -> CanvasPanel_Tiles 로컬 좌표. 보드도 같은 함수를 쓴다. */
	static FVector2D IndexToPosition(int32 Index);

	/** 연출용 방향 계산 (From 에서 To 로 갈 때). 인접하지 않으면 None. */
	static EDRS2SlideDir ComputeDirection(int32 From, int32 To);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ===== BindWidget =====
	// Img_Tile / Button_Tile 은 없으면 조각이 성립하지 않으므로 필수.
	// 나머지 둘은 Optional 이라 하이어라키를 단순화해도 컴파일이 깨지지 않는다.

	/** 흔들림·눌림 연출의 대상. 없으면 위젯 자신에게 건다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> Overlay_Content;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_Tile;

	/** 마우스 오버 하이라이트. BP 가 직접 연출해도 되고, 비워 두면 코드가 토글한다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Hover;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Tile;

	// ===== BindWidgetAnim (전부 선택) =====
	// 만들어 두면 코드 연출 대신 이쪽이 재생된다. 없어도 동작한다.

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Invalid;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Press;

	// ===== 튜닝값 (WBP Class Defaults 에서 조정) =====

	/** 0~1 진행률을 0~1 완료율로 바꾸는 이징 커브. 비우면 InterpEaseOut(exp 2) 로 대체된다. */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle|Slide")
	TObjectPtr<UCurveFloat> SlideCurve;

	/** 흔들림 총 길이(초). */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle|Shake", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ShakeDuration = 0.25f;

	/** 좌우 진폭(px, 1920x1080 기준). 셀 간격 2px 보다 커야 눈에 띈다. */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle|Shake", meta = (ClampMin = "1.0", ClampMax = "40.0"))
	float ShakeAmplitude = 7.f;

	/** 흔들림 왕복 횟수. 2 면 오른쪽-왼쪽-오른쪽-왼쪽으로 잦아든다. */
	UPROPERTY(EditDefaultsOnly, Category = "S2|Puzzle|Shake", meta = (ClampMin = "0.5", ClampMax = "6.0"))
	float ShakeCycles = 2.f;

	// ===== BP 연출 훅 =====
	// 전부 "이미 결정된 사실의 통보"다. 여기서 상태를 바꾸지 않는다.

	/** 미끄러짐 시작. 방향이 필요한 연출(스케일 늘이기, 잔상)에 쓴다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnSlideStarted(EDRS2SlideDir Direction);

	/** 미끄러짐 종료. 착지 사운드/파티클. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnSlideEnded();

	/** 갈 수 없는 조각을 눌렀다. 실패 사운드. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnInvalidMove();

	/** 마우스 오버 상태 변화. Img_Hover 를 비워 두고 BP 에서 직접 그려도 된다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnHoverChanged(bool bHovered);

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

private:
	void TickSlide(float DeltaTime);
	void TickShake(float DeltaTime);

	/** Canvas Slot 이 아직 없으면 조용히 무시한다 (디자이너 프리뷰 등). */
	void SetSlotPosition(const FVector2D& InPosition);
	bool GetSlotPosition(FVector2D& OutPosition) const;

	/** 흔들림 오프셋을 적용할 위젯. Overlay_Content 가 있으면 그쪽, 없으면 자기 자신. */
	UWidget* GetMotionTarget();

	int32 TileId    = 0;
	int32 GridIndex = INDEX_NONE;

	EDRS2TileMotion Motion = EDRS2TileMotion::Idle;

	// 미끄러짐 상태
	FVector2D SlideStartPos = FVector2D::ZeroVector;
	FVector2D SlideEndPos   = FVector2D::ZeroVector;
	float     SlideElapsed  = 0.f;
	float     SlideDuration = 0.16f;

	// 흔들림 상태
	float ShakeElapsed = 0.f;
};
