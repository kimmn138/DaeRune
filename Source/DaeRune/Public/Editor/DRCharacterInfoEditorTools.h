// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRCharacterInfoEditorTools.generated.h"

/**
 * 캐릭터 정보창(조작법 화면, HUD::ShowCharacterInfo) WBP 를 다시 짓는 에디터 자동화 — ★에디터 전용★.
 * 게임 빌드에서는 본문이 비어 있다. UDRStageSelectEditorTools 와 같은 방식이다.
 *
 * 배경 그림(UI-CharacterInfo-<캐릭터>)에서 글자 · 조작 버튼 · 물 사용량 칸이 빠졌다.
 * 그래서 스킬 이름 옆의 조작 버튼(SkillIcon_LMB/RMB/E/Q)과 물 사용량 칸을 위젯으로 두고, 이름 뒤에 붙인다(HorizontalBox)
 * — 번역으로 이름이 길어져도 버튼이 글자를 덮지 않는다.
 *   - 물 사용량 칸(Cost_SkillN)은 아직 그림이 없어서 숫자만 넣고 Collapsed 로 둔다. 그림이 오면 Image_SkillNCostBg 브러시를 넣고 보이게 한다.
 *   - 배경 그림은 예전 그림(4386×2256)에 투명 여백을 붙인 5400×2829 다. 예전과 같은 배율(1536/4386)로 그린다 → 예전 글자 좌표를 여백만큼 옮겨 그대로 쓴다.
 *   - 예전 WBP 의 글자(FText)를 위젯 이름으로 찾아 그대로 옮긴다 → 번역 키가 유지된다. 처음 생기는 글자만 새 키를 받는다.
 *     표(Specs)에 Korean 이 적힌 글자는 표가 정본이다 — 원문이 다르면 새 키로 바꾼다(그 뒤 Gather · 번역 · Compile).
 *   - 스킬 설명은 RichTextBlock 이다. 글자 속 <img id="LMB"/>(RMB · E · Q)가 조작 버튼 그림이 된다
 *     (BP_CharacterInfoKeyDecorator → DT_CharacterInfoKeyImages, 둘 다 미리 있어야 하고 표 줄은 이 도구가 채운다).
 *
 * Python(에디터 스크립팅)에서 부른다 — WBP 에셋은 미리 있어야 한다(부모 UserWidget):
 *   log = unreal.DRCharacterInfoEditorTools.build_character_info_widget('Gardener')        # WBP_GardenInfo
 *   log = unreal.DRCharacterInfoEditorTools.build_character_info_widget('VendingMachine')  # WBP_VendingInfo
 *   log = unreal.DRCharacterInfoEditorTools.build_character_info_widget('RobotVacuum')     # WBP_VacuumInfo
 * 트리는 ★매번 통째로 다시 만든다★ — 몇 번을 돌려도 같은 결과(멱등). 디자이너가 손으로 고친 배치는 덮어써진다.
 * 컴파일까지 하고, 저장은 부른 쪽(Python)이 한다.
 */
UCLASS()
class DAERUNE_API UDRCharacterInfoEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 성공하면 로그 첫 줄이 "OK", 실패하면 "ERROR" 로 시작한다.
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static FString BuildCharacterInfoWidget(const FString& CharacterName);
};
