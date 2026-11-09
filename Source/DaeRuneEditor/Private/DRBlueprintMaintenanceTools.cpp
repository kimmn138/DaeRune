// Copyright DaeRune


#include "DRBlueprintMaintenanceTools.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "DaeRune/DRLogChannels.h"

namespace
{
	TArray<UEdGraph*> GetGraphs(UBlueprint* Blueprint)
	{
		TArray<UEdGraph*> Graphs;
		if (Blueprint)
		{
			Blueprint->GetAllGraphs(Graphs);
		}
		return Graphs;
	}

	UEdGraphNode* FindNodeByName(UBlueprint* Blueprint, const FString& NodeName)
	{
		for (UEdGraph* Graph : GetGraphs(Blueprint))
		{
			if (!Graph) continue;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node && Node->GetName() == NodeName) return Node;
			}
		}
		return nullptr;
	}
}

FString UDRBlueprintMaintenanceTools::DescribeGraphs(UBlueprint* Blueprint, const FString& Filter)
{
	if (!Blueprint) return TEXT("Blueprint 없음");

	FString Out;
	for (UEdGraph* Graph : GetGraphs(Blueprint))
	{
		if (!Graph) continue;
		Out += FString::Printf(TEXT("## Graph %s (%d nodes)\n"), *Graph->GetName(), Graph->Nodes.Num());

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			const FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
			if (!Filter.IsEmpty() && !Node->GetName().Contains(Filter) && !Title.Contains(Filter)) continue;

			Out += FString::Printf(TEXT("- %s [%s] \"%s\"\n"), *Node->GetName(), *Node->GetClass()->GetName(), *Title);
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->LinkedTo.Num() == 0) continue;

				FString Links;
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked && Linked->GetOwningNodeUnchecked())
					{
						Links += FString::Printf(TEXT("%s.%s "), *Linked->GetOwningNodeUnchecked()->GetName(), *Linked->PinName.ToString());
					}
				}
				Out += FString::Printf(TEXT("    %s %s -> %s\n"),
					Pin->Direction == EGPD_Input ? TEXT("in ") : TEXT("out"), *Pin->PinName.ToString(), *Links);
			}
		}
	}
	return Out;
}

TArray<FString> UDRBlueprintMaintenanceTools::FindFunctionCallNodes(UBlueprint* Blueprint, FName FunctionName)
{
	TArray<FString> Found;
	for (UEdGraph* Graph : GetGraphs(Blueprint))
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
			if (CallNode && CallNode->FunctionReference.GetMemberName() == FunctionName)
			{
				Found.Add(FString::Printf(TEXT("%s/%s"), *Graph->GetName(), *CallNode->GetName()));
			}
		}
	}
	return Found;
}

int32 UDRBlueprintMaintenanceTools::RemoveNodesByName(UBlueprint* Blueprint, const TArray<FString>& NodeNames)
{
	if (!Blueprint) return 0;

	TArray<UEdGraphNode*> ToRemove;
	for (UEdGraph* Graph : GetGraphs(Blueprint))
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && NodeNames.Contains(Node->GetName()))
			{
				ToRemove.Add(Node);
			}
		}
	}

	for (UEdGraphNode* Node : ToRemove)
	{
		UE_LOG(LogDR, Display, TEXT("[BPMaintenance] %s: %s 삭제"), *Blueprint->GetName(), *Node->GetName());
		FBlueprintEditorUtils::RemoveNode(Blueprint, Node, /*bDontRecompile*/ true);
	}

	if (ToRemove.Num() > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
	return ToRemove.Num();
}

bool UDRBlueprintMaintenanceTools::RemoveMemberVariable(UBlueprint* Blueprint, FName VariableName)
{
	if (!Blueprint || FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableName) == INDEX_NONE)
	{
		return false;
	}

	FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VariableName);
	UE_LOG(LogDR, Display, TEXT("[BPMaintenance] %s: 변수 %s 삭제"), *Blueprint->GetName(), *VariableName.ToString());
	return true;
}

FString UDRBlueprintMaintenanceTools::AddVariableSetNode(UBlueprint* Blueprint, FName VariableName,
	const FString& ExecFromNode, FName ExecFromPin, const FString& ValueFromNode, FName ValueFromPin)
{
	if (!Blueprint || FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableName) == INDEX_NONE) return FString();

	UEdGraphNode* ExecNode = FindNodeByName(Blueprint, ExecFromNode);
	UEdGraphNode* ValueNode = FindNodeByName(Blueprint, ValueFromNode);
	if (!ExecNode || !ValueNode || ExecNode->GetGraph() != ValueNode->GetGraph()) return FString();

	UEdGraphPin* ExecOut = ExecNode->FindPin(ExecFromPin, EGPD_Output);
	UEdGraphPin* ValueOut = ValueNode->FindPin(ValueFromPin, EGPD_Output);
	if (!ExecOut || !ValueOut) return FString();

	UEdGraph* Graph = ExecNode->GetGraph();
	const UEdGraphSchema* Schema = Graph->GetSchema();

	// 변수 참조를 먼저 정해야 Finalize(AllocateDefaultPins)가 그 변수 타입의 값 핀을 만든다
	FGraphNodeCreator<UK2Node_VariableSet> Creator(*Graph);
	UK2Node_VariableSet* SetNode = Creator.CreateNode();
	SetNode->VariableReference.SetSelfMember(VariableName);
	SetNode->NodePosX = ExecNode->NodePosX + 320;
	SetNode->NodePosY = ExecNode->NodePosY;
	Creator.Finalize();

	// 실행 출력은 한 줄만 이을 수 있다 — 원래 뒤에 있던 것은 새 노드의 then 으로 옮긴다
	const TArray<UEdGraphPin*> OldExecTargets = ExecOut->LinkedTo;

	UEdGraphPin* ExecIn = SetNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
	UEdGraphPin* ThenOut = SetNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
	UEdGraphPin* ValueIn = SetNode->FindPin(VariableName, EGPD_Input);
	bool bLinked = ExecIn && ThenOut && ValueIn
		&& Schema->TryCreateConnection(ExecOut, ExecIn)
		&& Schema->TryCreateConnection(ValueOut, ValueIn);
	for (UEdGraphPin* Target : OldExecTargets)
	{
		bLinked = bLinked && Schema->TryCreateConnection(ThenOut, Target);
	}

	if (!bLinked)
	{
		UE_LOG(LogDR, Warning, TEXT("[BPMaintenance] %s: Set %s 연결 실패 — 노드를 지운다 (원래 연결은 복구되지 않을 수 있다)"),
			*Blueprint->GetName(), *VariableName.ToString());
		FBlueprintEditorUtils::RemoveNode(Blueprint, SetNode, /*bDontRecompile*/ true);
		return FString();
	}

	UE_LOG(LogDR, Display, TEXT("[BPMaintenance] %s: %s(Set %s) 추가 — %s.%s 뒤, 값 %s.%s"), *Blueprint->GetName(), *SetNode->GetName(),
		*VariableName.ToString(), *ExecFromNode, *ExecFromPin.ToString(), *ValueFromNode, *ValueFromPin.ToString());
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return SetNode->GetName();
}
