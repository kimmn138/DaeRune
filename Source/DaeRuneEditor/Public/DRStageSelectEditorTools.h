// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRStageSelectEditorTools.generated.h"

/**
 * 스테이지 선택 화면 에디터 자동화 — ★에디터 전용★. (아트: Figma "대룬 ui > 스테이지", 2026-10-29)
 *
 * Python(에디터 스크립팅)에서 부른다:
 *   log = unreal.DRStageSelectEditorTools.build_stage_select_card_widget()
 *   log = unreal.DRStageSelectEditorTools.build_stage_select_screen_widget()
 *
 * WBP 에셋은 미리 만들어 둬야 한다(부모 클래스 UDRStageSelectCardWidget / UDRStageSelectScreenWidget).
 * 트리는 ★매번 통째로 다시 만든다★ — 몇 번을 돌려도 같은 결과(멱등). 디자이너가 손으로 고친 배치는 덮어써진다.
 * 좌표 · 글자 크기는 Figma 프레임(1920x1080)을 100% 로 떠서 잰 값이다 (cpp 의 Spec 주석).
 * 카드 그림은 스테이지마다 다르므로 데이터(DA_ProgressionConfig > StageSelectList > CardArt)에서 온다 —
 * WBP 에 넣는 브러시는 디자이너 미리보기용 스테이지 1 그림이다.
 * 저장은 하지 않는다 — 부른 쪽(Python)이 저장한다.
 */
UCLASS()
class DAERUNEEDITOR_API UDRStageSelectEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// WBP_StageSelectCard — 베이스 · 그림 · 테두리 · CLEARED 도장 · 번호 · 이름 · 배지 · 임무 · 자물쇠 + LockBlendMaterial
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool BuildStageSelectCardWidget(FString& OutLog);

	// WBP_StageSelect — 어둡게 덮기 · 제목 · 카드 줄 · ESC BACK / ENTER START 키 안내 + CardWidgetClass
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool BuildStageSelectScreenWidget(FString& OutLog);

	/**
	 * WBP 안 TextBlock 하나의 디자이너 글자를 바꾼다 (Python 은 위젯 트리를 못 만진다).
	 * 예) 포털 안내 WBP_StageSelectInteraction 의 TextBlock_Interaction — 화면이 생기면서 "스테이지1로 가려면" 문구가 틀려졌다.
	 * 같은 네임스페이스/키를 유지하므로 번역 수집이 끊기지 않는다(키가 없던 글자는 새 키). 저장은 부른 쪽이 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool SetWidgetBlueprintText(const FString& WidgetBlueprintPath, FName WidgetName, const FString& NewText, FString& OutLog);
};
