// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "DRStageSelectTypes.generated.h"

class UTexture2D;
class UWorld;

/**
 * 스테이지 선택 카드 1장의 상태. (시안: 스테이지 선택.html 의 normal / cleared / locked)
 *
 * 판정은 ★호스트의 로컬 세이브★ 기준이다 — 스테이지를 고르는 사람이 호스트뿐이기 때문이다.
 * 클리어 여부는 별도 필드 없이 1회성 보상 원장의 "StageClear.<StageId>" 로 파생한다.
 */
UENUM(BlueprintType)
enum class EDRStageSelectState : uint8
{
	Locked,		// 선행 스테이지 미클리어 — 선택 불가 (누르면 카드가 흔들린다)
	Available,	// 진행 가능 (아직 클리어 안 함)
	Cleared		// 클리어 완료 — 다시 플레이 가능 (CLEARED 도장)
};

/**
 * 카드 1장의 그림 묶음 — 아트 팀 Figma "대룬 ui > 스테이지" 의 레이어 (파일 레이어가이드).
 * 카드마다 모양이 달라서(1번: 위 계단 + 왼쪽 홈, 2번: 오른쪽 홈) 스테이지마다 한 벌씩 둔다.
 *
 * ★텍스처 규격★ — 들여올 때 맞춰 둔다 (2026-10-29 Stage1/2 는 이 규격으로 가공해 들였다)
 *  - Base · Outline · SelectBorder · LockedBase · LockedOutline 은 모두 "글로우 캔버스" 다:
 *    카드 모양(CardSize) 사방에 50px 여백. 카드 위젯은 다섯 장 모두 카드 왼쪽 위 (-50,-50) 에 원래 크기로 그린다.
 *  - 잠금 레이어 두 장에는 Figma 의 "잠금 효과"(블렌드 모드 색상)를 미리 구워 두었다.
 *    같은 효과가 그림 · 배지에는 런타임 머티리얼(M_UI_StageSelect_LockBlend)로 걸린다.
 */
USTRUCT(BlueprintType)
struct FDRStageCardArt
{
	GENERATED_BODY()

	// 카드 모양의 크기(px). 레이아웃 크기다 — 글로우는 이 밖으로 번진다. 카드들은 아래쪽이 맞춰진다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	FVector2D CardSize = FVector2D(787.0, 661.0);

	// 기본 베이스 — 어두운 판 + 바깥 글로우 (기본 · 선택 상태)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	TSoftObjectPtr<UTexture2D> Base;

	// 기본 외곽선 — 선택되지 않은 카드
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	TSoftObjectPtr<UTexture2D> Outline;

	// 선택 테두리 — 선택된 카드. 그림 위에 그린다 (안쪽 빛 번짐이 그림에 얹힌다)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	TSoftObjectPtr<UTexture2D> SelectBorder;

	// 잠금용 베이스 — 잠긴 카드 (잠금 효과가 구워져 있다)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	TSoftObjectPtr<UTexture2D> LockedBase;

	// 잠금용 외곽 — 잠긴 카드 (잠금 효과가 구워져 있다)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	TSoftObjectPtr<UTexture2D> LockedOutline;

	// 큰 번호 그림 ("01") — 높이는 카드 위젯의 NumberHeight, 가로는 그림 비율대로
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	TSoftObjectPtr<UTexture2D> NumberImage;

	// 이름 옆 "스테이지 N" 배지 바탕 — 원래 크기(1x)로 그린다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	TSoftObjectPtr<UTexture2D> BadgeImage;

	// 번호 · 글자 묶음을 카드마다 옮기는 값(px). Figma 에서 2번 카드의 내용은 1번보다 5px 오른쪽에 있다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Card")
	FVector2D ContentOffset = FVector2D::ZeroVector;

	bool HasStateLayers() const { return !Base.IsNull() && !Outline.IsNull() && !SelectBorder.IsNull(); }
	bool HasLockedLayers() const { return !LockedBase.IsNull() && !LockedOutline.IsNull(); }
};

/**
 * 스테이지 선택 화면에 나오는 스테이지 1개의 정의. DA_ProgressionConfig 의 StageSelectList 에 순서대로 넣는다.
 * ★배열 순서 = 화면 순서 = 스테이지 번호(1부터)★ 다.
 */
USTRUCT(BlueprintType)
struct FDRStageSelectDef
{
	GENERATED_BODY()

	// ADRStageGameMode::StageId 와 같아야 한다. ★StageRewards 에도 같은 Id 가 있어야 클리어가 기록된다★
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage")
	FName StageId;

	// 카드에 크게 나오는 스테이지 이름 ("연구 격납고")
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage")
	FText DisplayName;

	// 카드 하단 임무 설명 ("적들로부터 클렌저를 지키세요")
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage", meta = (MultiLine = true))
	FText MissionText;

	// 시작하면 이동할 맵. 문자열 대신 에셋 참조라 맵 이름 오타가 생기지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage")
	TSoftObjectPtr<UWorld> Map;

	// 이 스테이지가 열리려면 먼저 클리어해야 하는 스테이지. 비우면 처음부터 열려 있다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage")
	FName RequiredStageId;

	// 카드 그림 (Figma "스테이지N 그림" 내보내기 그대로 — 카드 기준 (-13,-1) 에 원래 크기로 그린다)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	// 카드 테두리 · 베이스 · 번호 · 배지 그림
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage")
	FDRStageCardArt CardArt;
};

/**
 * 카드 위젯이 그리는 데 필요한 모든 정보. UDRGameInstance::GetStageSelectViewModels 가 만든다.
 * 선택/호버 같은 ★화면 상태는 여기 없다★ — 그건 화면 위젯이 들고 있는다.
 */
USTRUCT(BlueprintType)
struct FDRStageSelectViewModel
{
	GENERATED_BODY()

	// StageSelectList 안의 인덱스 (0부터)
	UPROPERTY(BlueprintReadOnly, Category = "StageSelect")
	int32 Index = INDEX_NONE;

	// 표시용 번호 (1부터)
	UPROPERTY(BlueprintReadOnly, Category = "StageSelect")
	int32 StageNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect")
	FName StageId;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect")
	FText MissionText;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect")
	EDRStageSelectState State = EDRStageSelectState::Locked;

	// 잠긴 이유 ("스테이지 1 클리어 후 열립니다.") — 잠기지 않았으면 비어 있다
	UPROPERTY(BlueprintReadOnly, Category = "StageSelect")
	FText LockHint;

	// 로드된 카드 그림. 지정이 없으면 nullptr
	UPROPERTY(BlueprintReadOnly, Category = "StageSelect")
	TObjectPtr<UTexture2D> Thumbnail = nullptr;

	// 카드 그림 묶음 (정의에서 그대로 복사 — 텍스처는 카드 위젯이 필요할 때 불러온다)
	UPROPERTY(BlueprintReadOnly, Category = "StageSelect")
	FDRStageCardArt CardArt;

	bool IsSelectable() const { return State != EDRStageSelectState::Locked; }
};
