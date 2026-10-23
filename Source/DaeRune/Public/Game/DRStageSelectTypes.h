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
	Locked,		// 선행 스테이지 미클리어 — 선택 불가 (흔들림 + 경고 문구)
	Available,	// 진행 가능 (아직 클리어 안 함)
	Cleared		// 클리어 완료 — 다시 플레이 가능
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

	// 카드 제목의 이름 부분 ("스테이지 1 · 연구 격납고" 의 "연구 격납고")
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

	// 카드 배경 이미지 (아트 미정 — 비우면 WBP 의 기본 브러시가 그대로 보인다)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage")
	TSoftObjectPtr<UTexture2D> Thumbnail;
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

	// 로드된 배경 이미지. 지정이 없으면 nullptr
	UPROPERTY(BlueprintReadOnly, Category = "StageSelect")
	TObjectPtr<UTexture2D> Thumbnail = nullptr;

	bool IsSelectable() const { return State != EDRStageSelectState::Locked; }
};
