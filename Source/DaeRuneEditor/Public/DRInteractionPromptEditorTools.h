// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRInteractionPromptEditorTools.generated.h"

/**
 * 화면 상호작용 안내(WBP_InteractionPrompt) 에디터 자동화 — ★에디터 전용★. (시안: 2026-11-08 "부품 떨어뜨리기" 캡처)
 *
 * Python(에디터 스크립팅)에서 부른다:
 *   log = unreal.DRInteractionPromptEditorTools.build_interaction_prompt_widget()
 *
 * WBP 에셋은 미리 만들어 둬야 한다(부모 클래스 UDRInteractionPromptWidget).
 * 트리는 ★매번 통째로 다시 만든다★ — 몇 번을 돌려도 같은 결과(멱등). 손으로 고친 배치는 덮어써진다.
 * 그림(KeyIcons · CarriedPartIcon)은 클래스 기본값이라 다시 지어도 그대로 남는다.
 * 저장은 하지 않는다 — 부른 쪽(Python)이 저장한다.
 */
UCLASS()
class DAERUNEEDITOR_API UDRInteractionPromptEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// [키캡][문구] 줄 + 부품 아이콘 자리 (UDRInteractionPromptWidget 의 BindWidget 이름 그대로)
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool BuildInteractionPromptWidget(FString& OutLog);
};
