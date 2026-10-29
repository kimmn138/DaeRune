// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Tuning/DRTuningTypes.h"

/**
 * 튜닝 시트(튜닝.csv) 읽기 · 쓰기.
 *
 * 엑셀이 바로 여는 형식으로 쓴다 — UTF-8(BOM) · 쉼표 구분 · CRLF.
 * 읽을 때는 칸 순서가 아니라 머리줄의 칸 이름으로 찾으므로 기획자가 칸을 옮겨도 된다.
 * 엑셀이 예전 방식(CP949)으로 다시 저장한 파일도 읽는다 (윈도우).
 */
namespace DRTuningSheet
{
	/** 파일을 글자로 — BOM(UTF-8/UTF-16) · BOM 없는 UTF-8 · CP949 를 가린다. 다른 프로그램이 열고 있어도 읽는다 */
	DAERUNE_API bool LoadFileText(const FString& FilePath, FString& OutText);

	/** 시트 글자 → 줄들. 머리줄([항목] 칸)이 없으면 false */
	DAERUNE_API bool ParseSheet(const FString& Text, TArray<FDRTuningRow>& OutRows, FString& OutError);

	/** 줄들 → 시트 글자 (머리줄 포함) */
	DAERUNE_API FString BuildSheetText(const TArray<FDRTuningRow>& Rows);

	/** UTF-8(BOM) 으로 저장. 엑셀이 파일을 잡고 있으면 false */
	DAERUNE_API bool SaveText(const FString& FilePath, const FString& Text);

	/** [변경값] 이 채워진 줄만 튜닝 값으로. OutLineNumbers 는 같은 순서의 파일 줄 번호 */
	DAERUNE_API void ExtractOverrides(const TArray<FDRTuningRow>& Rows, TArray<FDRTuningOverride>& OutOverrides, TArray<int32>& OutLineNumbers);

	/** 목록(Catalog)에 기존 시트의 [변경값] · [메모] 를 옮겨 담는다. 목록에 없는 줄이라도 변경값/메모가 있으면 끝에 남긴다 */
	DAERUNE_API void MergeSheet(const TArray<FDRTuningRow>& Catalog, const TArray<FDRTuningRow>& Existing, TArray<FDRTuningRow>& OutMerged);

	/** "0.01 ~ 5" → 최소 · 최대 (한쪽만 있어도 된다) */
	DAERUNE_API bool ParseRange(const FString& RangeText, TOptional<double>& OutMin, TOptional<double>& OutMax);
}
