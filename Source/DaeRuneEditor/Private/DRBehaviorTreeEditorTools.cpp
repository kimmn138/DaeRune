// Copyright DaeRune


#include "DRBehaviorTreeEditorTools.h"
#include "AIGraphTypes.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "ScopedTransaction.h"
#include "DaeRune/DRLogChannels.h"

#define LOCTEXT_NAMESPACE "DRBehaviorTreeEditorTools"

UBTTaskNode* UDRBehaviorTreeEditorTools::InsertTaskBefore(UBehaviorTree* Tree, const FString& BeforeNodeName, TSubclassOf<UBTTaskNode> TaskClass, int32 ShiftX)
{
	if (!Tree || !TaskClass) return nullptr;

	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
	if (!Graph)
	{
		UE_LOG(LogDR, Warning, TEXT("[BTEditor] %s: BT 그래프가 없다"), *Tree->GetName());
		return nullptr;
	}

	UBehaviorTreeGraphNode* BeforeNode = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(Node);
		if (BTNode && BTNode->NodeInstance && BTNode->NodeInstance->GetName() == BeforeNodeName)
		{
			BeforeNode = BTNode;
			break;
		}
	}

	UEdGraphPin* BeforeInput = BeforeNode ? BeforeNode->GetInputPin() : nullptr;
	if (!BeforeInput || BeforeInput->LinkedTo.Num() != 1)
	{
		UE_LOG(LogDR, Warning, TEXT("[BTEditor] %s: 부모에 이어진 노드 %s 를 찾지 못했다"), *Tree->GetName(), *BeforeNodeName);
		return nullptr;
	}
	UEdGraphPin* ParentOutput = BeforeInput->LinkedTo[0];

	const FScopedTransaction Transaction(LOCTEXT("InsertTask", "Insert BT Task"));
	Graph->Modify();

	// 자리 만들기 — X 가 기준 노드 이상인 노드를 모두 같은 만큼 밀면 어느 부모 아래서든 형제 순서가 그대로다
	const int32 InsertX = BeforeNode->NodePosX;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;

		if (UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
		{
			if (Comment->NodePosX >= InsertX)
			{
				Comment->Modify();
				Comment->NodePosX += ShiftX;
			}
			else if (Comment->NodePosX + Comment->NodeWidth > InsertX)
			{
				Comment->Modify();
				Comment->NodeWidth += ShiftX;
			}
			continue;
		}

		if (Node->NodePosX >= InsertX)
		{
			Node->Modify();
			Node->NodePosX += ShiftX;
		}
	}

	// 팔레트에서 노드를 놓을 때와 같은 순서 (FAISchemaAction_NewNode::PerformAction) — PostPlacedNewNode 가 노드 인스턴스를 BT 에셋 아래에 만든다
	UBehaviorTreeGraphNode_Task* NewNode = NewObject<UBehaviorTreeGraphNode_Task>(Graph);
	NewNode->ClassData = FGraphNodeClassData(TaskClass.Get(), FString());
	NewNode->SetFlags(RF_Transactional);
	Graph->AddNode(NewNode, /*bUserAction*/ false, /*bSelectNewNode*/ false);
	NewNode->CreateNewGuid();
	NewNode->PostPlacedNewNode();
	NewNode->AllocateDefaultPins();
	NewNode->NodePosX = InsertX;
	NewNode->NodePosY = BeforeNode->NodePosY;

	UEdGraphPin* NewInput = NewNode->GetInputPin();
	UBTTaskNode* NewTask = Cast<UBTTaskNode>(NewNode->NodeInstance);
	if (!NewInput || !NewTask)
	{
		UE_LOG(LogDR, Warning, TEXT("[BTEditor] %s: %s 노드를 만들지 못했다"), *Tree->GetName(), *TaskClass->GetName());
		Graph->RemoveNode(NewNode);
		return nullptr;
	}
	NewTask->SetFlags(RF_Transactional);

	ParentOutput->Modify();
	ParentOutput->MakeLinkTo(NewInput);

	Graph->UpdateAsset();
	Graph->NotifyGraphChanged();
	Tree->MarkPackageDirty();

	UE_LOG(LogDR, Display, TEXT("[BTEditor] %s: %s 앞에 %s 를 끼웠다"), *Tree->GetName(), *BeforeNodeName, *NewTask->GetName());
	return NewTask;
}

void UDRBehaviorTreeEditorTools::RefreshBehaviorTree(UBehaviorTree* Tree)
{
	UBehaviorTreeGraph* Graph = Tree ? Cast<UBehaviorTreeGraph>(Tree->BTGraph) : nullptr;
	if (!Graph) return;

	Graph->UpdateBlackboardChange();
	Graph->UpdateAsset();
	Graph->NotifyGraphChanged();
	Tree->MarkPackageDirty();
}

#undef LOCTEXT_NAMESPACE
