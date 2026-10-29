// Copyright DaeRune


#include "DRBlueprintMaintenanceTools.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
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
