// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRBehaviorTreeEditorTools.generated.h"

class UBehaviorTree;
class UBTTaskNode;

/**
 * 비헤이비어 트리 그래프 편집 도우미 (Python 원격 실행용).
 * BT 그래프 노드는 Python 에서 만들 수 없어 C++ 로 둔다.
 *
 *   tools = unreal.DRBehaviorTreeEditorTools
 *   node = tools.insert_task_before(bt, 'BTT_Attack_Dog_C_1', unreal.DRBTTask_FaceTarget)
 *   unreal.DRWardrobeEditorTools.import_property_text(node, 'BlackboardKey', '(SelectedKeyName="TargetToFollow")')
 *   tools.refresh_behavior_tree(bt)    # 그다음 저장
 */
UCLASS()
class DAERUNEEDITOR_API UDRBehaviorTreeEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 노드 인스턴스 이름이 BeforeNodeName 인 노드(예: BTT_Attack_Dog_C_1) 바로 앞 — 같은 부모 아래 실행 순서상 바로 앞 — 에
	 * TaskClass 태스크를 끼운다. 자식 실행 순서는 그래프 X 위치 순서라, 그 노드 자리에 새 노드를 두고
	 * 그 자리부터 오른쪽에 있는 노드는 모두 ShiftX 만큼 민다(형제 순서는 그대로, 걸쳐 있는 코멘트는 넓힌다).
	 * 새 태스크 노드 인스턴스를 돌려준다(실패면 None) — 속성은 Python 에서 넣고 RefreshBehaviorTree 를 부른다 (저장은 따로)
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static UBTTaskNode* InsertTaskBefore(UBehaviorTree* Tree, const FString& BeforeNodeName, TSubclassOf<UBTTaskNode> TaskClass, int32 ShiftX = 300);

	/** 그래프로 런타임 트리를 다시 만들고 노드마다 블랙보드 키를 다시 찾는다 (노드 속성을 바꾼 뒤 · 저장 전) */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static void RefreshBehaviorTree(UBehaviorTree* Tree);
};
