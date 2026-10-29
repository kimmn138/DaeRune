// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRBlueprintMaintenanceTools.generated.h"

class UBlueprint;

/**
 * BP 그래프 정리 도우미 (Python 원격 실행용).
 * C++ 에서 함수 · 이벤트 · 프로퍼티를 지우기 전에 BP 쪽 노드를 먼저 걷어낼 때 쓴다 — 순서가 바뀌면
 * BP 가 "없는 함수를 부른다"는 컴파일 오류 상태로 열린다.
 *
 * Python 에서는 UEdGraph::Nodes 같은 그래프 내부에 손이 닿지 않아 C++ 로 둔다.
 *   tools = unreal.DRBlueprintMaintenanceTools
 *   print(tools.describe_graphs(bp, ''))                 # 노드 · 핀 연결 확인
 *   tools.remove_nodes_by_name(bp, ['K2Node_CallFunction_2'])
 */
UCLASS()
class DAERUNEEDITOR_API UDRBlueprintMaintenanceTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 모든 그래프의 노드 이름 · 제목 · 핀 연결을 한 줄씩. Filter 가 비어 있지 않으면 이름/제목에 그 글자가 든 노드만 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static FString DescribeGraphs(UBlueprint* Blueprint, const FString& Filter);

	/** 이 BP 의 모든 그래프에서 FunctionName 을 부르는 노드 이름 (그래프/노드) */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static TArray<FString> FindFunctionCallNodes(UBlueprint* Blueprint, FName FunctionName);

	/** 노드 오브젝트 이름(K2Node_CallFunction_2 등)으로 지운다. 지운 개수를 돌려준다 (컴파일 · 저장은 따로) */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static int32 RemoveNodesByName(UBlueprint* Blueprint, const TArray<FString>& NodeNames);

	/** BP 멤버 변수를 지운다 (그 변수를 읽고 쓰는 노드는 먼저 지울 것). 없으면 false */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool RemoveMemberVariable(UBlueprint* Blueprint, FName VariableName);
};
