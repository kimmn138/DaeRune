// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRStageSelectEditorTools.generated.h"

/**
 * 스테이지 선택 화면(시안: 스테이지 선택.html) 에디터 자동화 — ★에디터 전용★.
 * 게임 빌드에서는 본문이 비어 있다. UDRWardrobeEditorTools 와 같은 방식이다.
 *
 * Python(에디터 스크립팅)에서 부른다:
 *   log = unreal.DRStageSelectEditorTools.build_stage_select_card_widget()
 *   log = unreal.DRStageSelectEditorTools.build_stage_select_screen_widget()
 *
 * WBP 에셋은 미리 만들어 둬야 한다(부모 클래스 UDRStageSelectCardWidget / UDRStageSelectScreenWidget).
 * 트리는 ★매번 통째로 다시 만든다★ — 몇 번을 돌려도 같은 결과(멱등). 디자이너가 손으로 고친 배치는 덮어써진다.
 * 이미지 아트가 아직 없어서 전부 색 브러시 + 머티리얼(배경/썸네일 그라데이션)로 시안을 재현한다.
 * 저장은 하지 않는다 — 부른 쪽(Python)이 저장한다.
 */
UCLASS()
class DAERUNE_API UDRStageSelectEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// WBP_StageSelectCard — 테두리 · 썸네일 · 번호 · 배지 · CLEARED 도장 · 자물쇠 · 정보 영역
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool BuildStageSelectCardWidget(FString& OutLog);

	// WBP_StageSelect — 배경 · 머리말(제목/클리어 수) · 카드 영역 · 바닥글(문구/뒤로/시작) + CardWidgetClass
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
