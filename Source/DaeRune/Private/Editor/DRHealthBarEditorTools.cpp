// Copyright DaeRune


#include "Editor/DRHealthBarEditorTools.h"

#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/DRHealthUILibrary.h"
#endif

#if WITH_EDITOR
namespace DRHealthBarEditor
{
	const TCHAR* HealthWaterBarWBPPath = TEXT("/Game/Blueprints/UI/Container/WBP_HealthWaterBar.WBP_HealthWaterBar");

	const FName MaxHealthVarName(TEXT("MaxHealth"));
	const FName IsCorruptedVarName(TEXT("IsCorrupted"));
	const FName RingParameterName(TEXT("CurrentHP"));

	FName ToDisplayFuncName() { return GET_FUNCTION_NAME_CHECKED(UDRHealthUILibrary, ToContainerDisplayHealth); }
	FName IsCorruptedFuncName() { return GET_FUNCTION_NAME_CHECKED(UDRHealthUILibrary, IsOwningPlayerCorrupted); }

	UEdGraph* FindGraph(const TArray<TObjectPtr<UEdGraph>>& Graphs, FName Name)
	{
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && Graph->GetFName() == Name) return Graph;
		}
		return nullptr;
	}

	bool IsCallTo(const UEdGraphNode* Node, FName FunctionName)
	{
		const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
		return Call && Call->FunctionReference.GetMemberName() == FunctionName;
	}

	UK2Node_CallFunction* SpawnLibraryCall(UEdGraph* Graph, FName FunctionName, int32 X, int32 Y)
	{
		UFunction* Function = UDRHealthUILibrary::StaticClass()->FindFunctionByName(FunctionName);
		if (!Function) return nullptr;

		FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
		UK2Node_CallFunction* Node = Creator.CreateNode();
		Node->SetFromFunction(Function);
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Creator.Finalize();
		return Node;
	}

	UK2Node_VariableGet* SpawnSelfGet(UEdGraph* Graph, FName VarName, int32 X, int32 Y)
	{
		FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);
		UK2Node_VariableGet* Node = Creator.CreateNode();
		Node->VariableReference.SetSelfMember(VarName);
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Creator.Finalize();
		return Node;
	}

	bool Link(UEdGraphPin* A, UEdGraphPin* B, const FString& What, FString& Log)
	{
		if (!A || !B)
		{
			Log += FString::Printf(TEXT("ERROR: 핀을 찾지 못했습니다 — %s\n"), *What);
			return false;
		}
		if (!GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A, B))
		{
			Log += FString::Printf(TEXT("ERROR: 연결 실패 — %s\n"), *What);
			return false;
		}
		return true;
	}

	/**
	 * Source(체력 값 출력 핀) 에 물린 Targets 를 ToContainerDisplayHealth(Source, MaxHealth) 의 결과로 옮긴다.
	 * 이미 변환 노드로 가는 연결은 건드리지 않는다(멱등).
	 */
	bool InsertDisplayConversion(UEdGraph* Graph, UEdGraphPin* Source, const TArray<UEdGraphPin*>& Targets,
		const FString& What, FString& Log, int32& OutInserted)
	{
		TArray<UEdGraphPin*> ToMove;
		for (UEdGraphPin* Target : Targets)
		{
			if (Target && !IsCallTo(Target->GetOwningNode(), ToDisplayFuncName()))
			{
				ToMove.Add(Target);
			}
		}
		if (ToMove.Num() == 0) return true;

		const UEdGraphNode* SourceNode = Source->GetOwningNode();
		const int32 X = SourceNode->NodePosX + 220;
		const int32 Y = SourceNode->NodePosY + 60;

		UK2Node_CallFunction* Convert = SpawnLibraryCall(Graph, ToDisplayFuncName(), X, Y);
		UK2Node_VariableGet* MaxGet = SpawnSelfGet(Graph, MaxHealthVarName, X - 200, Y + 120);
		if (!Convert || !MaxGet)
		{
			Log += TEXT("ERROR: 변환 노드를 만들지 못했습니다\n");
			return false;
		}

		bool bOk = Link(Source, Convert->FindPin(TEXT("Health"), EGPD_Input), What + TEXT(" → Health"), Log);
		bOk &= Link(MaxGet->GetValuePin(), Convert->FindPin(TEXT("MaxHealth"), EGPD_Input), What + TEXT(" MaxHealth"), Log);

		UEdGraphPin* Result = Convert->GetReturnValuePin();
		for (UEdGraphPin* Target : ToMove)
		{
			Source->BreakLinkTo(Target);
			bOk &= Link(Result, Target, What + TEXT(" → 사용처"), Log);
		}

		++OutInserted;
		return bOk;
	}

	/** 함수 그래프 안의 Health / PreviousHealth 읽기를 전부 400 기준 값으로 바꾼다. */
	bool ConvertHealthReads(UBlueprint* BP, FName GraphName, FString& Log)
	{
		UEdGraph* Graph = FindGraph(BP->FunctionGraphs, GraphName);
		if (!Graph)
		{
			Log += FString::Printf(TEXT("ERROR: %s 함수가 없습니다\n"), *GraphName.ToString());
			return false;
		}

		TArray<UK2Node_VariableGet*> Gets;
		Graph->GetNodesOfClass(Gets);

		int32 Inserted = 0;
		int32 Found = 0;
		for (UK2Node_VariableGet* Get : Gets)
		{
			const FName Var = Get->VariableReference.GetMemberName();
			if (Var != TEXT("Health") && Var != TEXT("PreviousHealth")) continue;

			UEdGraphPin* Out = Get->GetValuePin();
			if (!Out) continue;
			++Found;

			if (!InsertDisplayConversion(Graph, Out, Out->LinkedTo, GraphName.ToString() + TEXT(".") + Var.ToString(), Log, Inserted))
			{
				return false;
			}
		}

		Log += FString::Printf(TEXT("%s: 체력 읽기 %d개, 변환 추가 %d개\n"), *GraphName.ToString(), Found, Inserted);
		if (Found == 0)
		{
			Log += TEXT("ERROR: 체력 읽기 노드를 찾지 못했습니다\n");
			return false;
		}
		return true;
	}

	/** 이벤트 그래프의 링 재질 SetScalarParameterValue(CurrentHP, …) 값 → 400 기준 값. */
	bool ConvertRingParameter(UBlueprint* BP, FString& Log)
	{
		const FName SetScalarName(TEXT("SetScalarParameterValue"));
		int32 Found = 0;
		int32 Inserted = 0;

		for (UEdGraph* Graph : BP->UbergraphPages)
		{
			if (!Graph) continue;
			TArray<UK2Node_CallFunction*> Calls;
			Graph->GetNodesOfClass(Calls);

			for (UK2Node_CallFunction* Call : Calls)
			{
				if (Call->FunctionReference.GetMemberName() != SetScalarName) continue;

				const UEdGraphPin* NamePin = Call->FindPin(TEXT("ParameterName"), EGPD_Input);
				if (!NamePin || NamePin->LinkedTo.Num() > 0 || FName(*NamePin->DefaultValue) != RingParameterName) continue;

				UEdGraphPin* ValuePin = Call->FindPin(TEXT("Value"), EGPD_Input);
				if (!ValuePin || ValuePin->LinkedTo.Num() != 1) continue;
				++Found;

				UEdGraphPin* Source = ValuePin->LinkedTo[0];
				if (IsCallTo(Source->GetOwningNode(), ToDisplayFuncName())) continue;	// 이미 변환됨

				if (!InsertDisplayConversion(Graph, Source, { ValuePin }, TEXT("Ring.CurrentHP"), Log, Inserted))
				{
					return false;
				}
			}
		}

		Log += FString::Printf(TEXT("링 CurrentHP: %d곳, 변환 추가 %d개\n"), Found, Inserted);
		if (Found == 0)
		{
			Log += TEXT("ERROR: 링 재질 CurrentHP 설정 노드를 찾지 못했습니다\n");
			return false;
		}
		return true;
	}

	/** IsCorrupted? : IsCorrupted 변수에 넣는 값 → IsOwningPlayerCorrupted(Self). */
	bool ConvertCorruptedCheck(UBlueprint* BP, FString& Log)
	{
		UEdGraph* Graph = FindGraph(BP->FunctionGraphs, TEXT("IsCorrupted?"));
		if (!Graph)
		{
			Log += TEXT("ERROR: IsCorrupted? 함수가 없습니다\n");
			return false;
		}

		TArray<UK2Node_VariableSet*> Sets;
		Graph->GetNodesOfClass(Sets);

		for (UK2Node_VariableSet* Set : Sets)
		{
			if (Set->VariableReference.GetMemberName() != IsCorruptedVarName) continue;

			UEdGraphPin* ValuePin = Set->FindPin(IsCorruptedVarName, EGPD_Input);
			if (!ValuePin)
			{
				Log += TEXT("ERROR: IsCorrupted 설정 노드의 값 핀이 없습니다\n");
				return false;
			}

			if (ValuePin->LinkedTo.Num() == 1 && IsCallTo(ValuePin->LinkedTo[0]->GetOwningNode(), IsCorruptedFuncName()))
			{
				Log += TEXT("IsCorrupted?: 이미 변환됨\n");
				return true;
			}

			// 예전 비교 노드(최대 체력 ≤ 100)를 지운다
			TArray<UK2Node*> OldSources;
			for (UEdGraphPin* Linked : ValuePin->LinkedTo)
			{
				if (UK2Node* LinkedNode = Linked ? Cast<UK2Node>(Linked->GetOwningNode()) : nullptr)
				{
					OldSources.AddUnique(LinkedNode);
				}
			}
			ValuePin->BreakAllPinLinks();
			for (UK2Node* Old : OldSources)
			{
				if (Old && Old->IsNodePure())
				{
					FBlueprintEditorUtils::RemoveNode(BP, Old, /*bDontRecompile=*/true);
				}
			}

			UK2Node_CallFunction* Call = SpawnLibraryCall(Graph, IsCorruptedFuncName(), Set->NodePosX - 300, Set->NodePosY + 80);
			if (!Call)
			{
				Log += TEXT("ERROR: IsOwningPlayerCorrupted 를 찾지 못했습니다\n");
				return false;
			}
			if (!Link(Call->GetReturnValuePin(), ValuePin, TEXT("IsCorrupted"), Log)) return false;

			Log += TEXT("IsCorrupted?: 판정 교체\n");
			return true;
		}

		Log += TEXT("ERROR: IsCorrupted? 안에 IsCorrupted 설정 노드가 없습니다\n");
		return false;
	}
}
#endif

FString UDRHealthBarEditorTools::FixHealthWaterBarWidget()
{
#if WITH_EDITOR
	using namespace DRHealthBarEditor;

	UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, HealthWaterBarWBPPath);
	if (!BP)
	{
		return FString::Printf(TEXT("ERROR: %s 를 불러오지 못했습니다\n"), HealthWaterBarWBPPath);
	}

	BP->Modify();

	FString Log;
	bool bOk = ConvertHealthReads(BP, TEXT("SetContainerSize"), Log);
	bOk = bOk && ConvertHealthReads(BP, TEXT("ShowContainerReduceEffectAnimation"), Log);
	bOk = bOk && ConvertRingParameter(BP, Log);
	bOk = bOk && ConvertCorruptedCheck(BP, Log);
	if (!bOk) return TEXT("ERROR\n") + Log;

	FKismetEditorUtilities::CompileBlueprint(BP);
	if (BP->Status == BS_Error)
	{
		return TEXT("ERROR: 컴파일 오류\n") + Log;
	}

	BP->MarkPackageDirty();
	return TEXT("OK\n") + Log;
#else
	return TEXT("ERROR: 에디터 전용\n");
#endif
}
