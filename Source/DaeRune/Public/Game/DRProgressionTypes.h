// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "DRProgressionTypes.generated.h"

/**
 * 캐릭터(클래스) 단위 진행도. 클라이언트 로컬 세이브에 클래스별로 1개씩 저장된다.
 * (Plan2.md 4.1 참조)
 */
USTRUCT(BlueprintType)
struct FDRCharacterProgress
{
	GENERATED_BODY()

	// 현재 레벨 (1부터 시작)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	int32 Level = 1;

	// 현재 레벨에서 누적된 경험치 (다음 레벨까지의 진행)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	int32 CurrentXP = 0;

	// 통계용 — 총 누적 경험치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	int32 TotalXP = 0;
};

/**
 * 스테이지 결과 반영 후 UI 연출용 요약. ApplyStageResult가 반환한다.
 * (Plan2.md 7.4 참조)
 */
USTRUCT(BlueprintType)
struct FDRStageProgressResult
{
	GENERATED_BODY()

	// 이번에 획득한 경험치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	int32 XpGained = 0;

	// 지급 전 레벨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	int32 StartLevel = 1;

	// 지급 후 레벨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	int32 EndLevel = 1;

	// 지급 후, 현재 레벨에서의 누적 경험치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	int32 XpIntoCurrent = 0;

	// 다음 레벨까지 필요한 경험치 (MAX 레벨이면 0)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	int32 XpForNext = 0;

	// 이번 지급으로 레벨업이 발생했는가
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progress")
	bool bLeveledUp = false;
};
