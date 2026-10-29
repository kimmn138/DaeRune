// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "DRTuningTypes.generated.h"

/**
 * 튜닝 값 1건 — "이 에셋의 이 속성을 이 값으로" (Tuning/튜닝.csv 의 [변경값] 이 채워진 한 줄).
 * 방장 → 참가자 동기화(ADRPlayerController::ClientReceiveTuningChunk)에도 그대로 실어 보낸다.
 */
USTRUCT()
struct FDRTuningOverride
{
	GENERATED_BODY()

	// 에셋 경로(/Game/…/GA_X.GA_X_C) 또는 짧은 이름(GA_X)
	UPROPERTY()
	FString Target;

	// 속성 경로 — WaterCost / Damage.Value / Chips[CM.Stat.MaxHealth].Modifiers[0].Value / [RowName].Field
	UPROPERTY()
	FString Property;

	UPROPERTY()
	FString Value;
};

/** 시트의 [분류] 칸 규칙 — 에셋 경로에 PathContains 가 들어 있으면 Label. 앞에 있는 규칙이 먼저다 (DefaultGame.ini) */
USTRUCT()
struct FDRTuningGroupRule
{
	GENERATED_BODY()

	UPROPERTY()
	FString PathContains;

	UPROPERTY()
	FString Label;
};

/**
 * 시트(튜닝.csv) 한 줄. 칸 순서는 DRTuningSheet::BuildSheetText 가 정한다.
 * [현재값] · [범위] · [설명] · [경로] 는 게임이 다시 쓰고, [변경값] · [메모] 만 기획자 칸이다.
 */
struct FDRTuningRow
{
	FString Group;			// 분류
	FString Target;			// 대상 (짧은 이름 — GA_SeedCannon)
	FString Property;		// 항목 (속성 경로)
	FString Current;		// 현재값 (빌드 기준값)
	FString Override;		// 변경값 (비우면 기준값 그대로)
	FString Range;			// 범위 (에디터 ClampMin ~ ClampMax)
	FString Description;	// 설명 (C++ 주석 첫 줄)
	FString Note;			// 메모 (기획자 칸)
	FString Path;			// 경로 (대상의 전체 경로 — 짧은 이름이 겹쳐도 이 칸으로 찾는다)

	int32 LineNumber = 0;	// 읽어 들인 파일의 몇 번째 줄인가 (결과 보고용)
	int32 GroupOrder = 0;	// 정렬용 — 분류 규칙 순서
	int32 KindOrder = 0;	// 정렬용 — 스킬 → 투사체 → 효과 → 데이터
};
