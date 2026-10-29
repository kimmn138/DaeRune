// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRTuningEditorTools.generated.h"

/**
 * 튜닝 파일(Tuning/튜닝.csv — UDRTuningSubsystem) 에디터 도구 (콘솔 · Python 원격 실행용).
 *
 *   콘솔  DRTuningBake                      <프로젝트>/Tuning/튜닝.csv 의 변경값을 에셋에 반영 (저장은 직접)
 *         DRTuningBake D:/받은/튜닝.csv save clear   다른 파일 · 저장까지 · 반영한 변경값 비우기
 *   Python  unreal.DRTuningEditorTools.bake_tuning_sheet('', False, False)
 *
 * 기획자가 패키지 빌드에서 정한 값을 개발자가 에셋에 옮겨 적는 대신 이 도구로 한 번에 반영한다.
 * 실행 취소(Ctrl+Z)가 된다. 목록 파일(쿡용 TuningCatalog.csv)은 쿡할 때 자동으로 만들어진다 — ExportTuningCatalog 는 미리 볼 때만.
 */
UCLASS()
class DAERUNEEDITOR_API UDRTuningEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 쿡용 목록 파일(Content/Tuning/TuningCatalog.csv)을 지금 만든다. 결과 문구를 돌려준다 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Tuning")
	static FString ExportTuningCatalog();

	/**
	 * 시트의 [변경값] 을 실제 에셋(BP 기본값 · 데이터 에셋 · 표)에 쓴다. PIE 중에는 하지 않는다.
	 * @param SheetPath           비우면 <프로젝트>/Tuning/튜닝.csv
	 * @param bSavePackages       true 면 바뀐 에셋을 바로 저장
	 * @param bClearBakedValues   true 면 반영한 줄의 [변경값] 을 비우고 [메모] 에 반영 기록을 남긴다
	 * @return 줄마다 결과
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Tuning")
	static FString BakeTuningSheet(const FString& SheetPath, bool bSavePackages, bool bClearBakedValues);
};
