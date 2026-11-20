// Copyright DaeRune

#include "DRVFXEditorTools.h"

#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraDataInterface.h"
#include "Curves/RichCurve.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectHash.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystemFactoryNew.h"
#include "ShaderCompiler.h"
#include "UObject/Package.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#endif

#if WITH_EDITOR
namespace
{
	// 시스템 스크립트 + 이미터마다 모든 스크립트
	TArray<UNiagaraScript*> GatherScripts(UNiagaraSystem* System)
	{
		TArray<UNiagaraScript*> Scripts;
		Scripts.Add(System->GetSystemSpawnScript());
		Scripts.Add(System->GetSystemUpdateScript());
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			if (const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData())
			{
				Data->GetScripts(Scripts, false);
			}
		}
		Scripts.RemoveAll([](const UNiagaraScript* S) { return S == nullptr; });
		return Scripts;
	}

	UNiagaraGraph* GetEmitterGraph(const FVersionedNiagaraEmitterData* Data)
	{
		const UNiagaraScriptSource* Source = Data ? Cast<UNiagaraScriptSource>(Data->GraphSource) : nullptr;
		return Source ? Source->NodeGraph : nullptr;
	}

	const TCHAR* UsageName(ENiagaraScriptUsage Usage)
	{
		switch (Usage)
		{
		case ENiagaraScriptUsage::EmitterSpawnScript: return TEXT("EmitterSpawn");
		case ENiagaraScriptUsage::EmitterUpdateScript: return TEXT("EmitterUpdate");
		case ENiagaraScriptUsage::ParticleSpawnScript:
		case ENiagaraScriptUsage::ParticleSpawnScriptInterpolated: return TEXT("ParticleSpawn");
		case ENiagaraScriptUsage::ParticleUpdateScript: return TEXT("ParticleUpdate");
		case ENiagaraScriptUsage::ParticleEventScript: return TEXT("Event");
		case ENiagaraScriptUsage::ParticleSimulationStageScript: return TEXT("SimStage");
		case ENiagaraScriptUsage::SystemSpawnScript: return TEXT("SystemSpawn");
		case ENiagaraScriptUsage::SystemUpdateScript: return TEXT("SystemUpdate");
		default: return TEXT("Other");
		}
	}

	FString CurveKeysToString(const FRichCurve& Curve)
	{
		FString Out;
		for (const FRichCurveKey& Key : Curve.GetConstRefOfKeys())
		{
			Out += FString::Printf(TEXT("%s%.3g:%.3g"), Out.IsEmpty() ? TEXT("") : TEXT(";"), Key.Time, Key.Value);
		}
		return Out;
	}

	FString MaterialSummary(UMaterialInterface* Material)
	{
		if (!Material) return TEXT("None");
		const UMaterial* Base = Material->GetMaterial();
		return FString::Printf(TEXT("%s[blend=%d unlit=%d]"), *Material->GetPathName(), (int32)Material->GetBlendMode(),
			Base ? (Base->GetShadingModels().HasShadingModel(MSM_Unlit) ? 1 : 0) : -1);
	}

	void FinishSystemEdit(UNiagaraSystem* System, bool bForce)
	{
		System->RequestCompile(bForce);
		System->WaitForCompilationComplete(true, false);
		System->MarkPackageDirty();
	}
}
#endif

FString UDRVFXEditorTools::DescribeNiagaraSystem(UNiagaraSystem* System)
{
	FString Out;
#if WITH_EDITOR
	if (!System) return TEXT("null");
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		if (!Data) continue;
		FString Renderers;
		for (const UNiagaraRendererProperties* Renderer : Data->GetRenderers())
		{
			if (Renderer) Renderers += Renderer->GetClass()->GetName() + TEXT(" ");
		}
		Out += FString::Printf(TEXT("%s enabled=%d local=%d sim=%s renderers=%s\n"),
			*Handle.GetName().ToString(), Handle.GetIsEnabled() ? 1 : 0, Data->bLocalSpace ? 1 : 0,
			Data->SimTarget == ENiagaraSimTarget::GPUComputeSim ? TEXT("GPU") : TEXT("CPU"), *Renderers);
	}
#endif
	return Out;
}

int32 UDRVFXEditorTools::SetNiagaraEmittersLocalSpace(UNiagaraSystem* System, bool bLocalSpace)
{
	int32 Changed = 0;
#if WITH_EDITOR
	if (!System) return 0;
	System->Modify();
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		if (!Data || Data->bLocalSpace == bLocalSpace) continue;
		if (UNiagaraEmitter* Emitter = Handle.GetInstance().Emitter)
		{
			Emitter->Modify();
		}
		Data->bLocalSpace = bLocalSpace;
		++Changed;
	}
	if (Changed > 0)
	{
		System->RequestCompile(true);
		System->WaitForCompilationComplete(true, false);
		System->MarkPackageDirty();
	}
#endif
	return Changed;
}

bool UDRVFXEditorTools::SetNiagaraEmitterEnabled(UNiagaraSystem* System, const FString& EmitterName, bool bEnabled)
{
#if WITH_EDITOR
	if (!System) return false;
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (Handle.GetName().ToString() != EmitterName) continue;
		System->Modify();
		if (Handle.SetIsEnabled(bEnabled, *System, true))
		{
			System->WaitForCompilationComplete(true, false);
			System->MarkPackageDirty();
		}
		return true;
	}
#endif
	return false;
}

FString UDRVFXEditorTools::DumpNiagaraModuleConstants(UNiagaraSystem* System, const FString& NameFilter)
{
	FString Out;
#if WITH_EDITOR
	if (!System) return TEXT("null");
	for (UNiagaraScript* Script : GatherScripts(System))
	{
		const FNiagaraParameterStore& Store = Script->RapidIterationParameters;
		for (const FNiagaraVariableWithOffset& Var : Store.ReadParameterVariables())
		{
			const FString Name = Var.GetName().ToString();
			if (!NameFilter.IsEmpty() && !Name.Contains(NameFilter)) continue;
			const uint8* Data = Store.GetParameterData(Var.Offset);
			FString Value = TEXT("?");
			if (Var.GetType() == FNiagaraTypeDefinition::GetFloatDef()) Value = FString::SanitizeFloat(*(const float*)Data);
			else if (Var.GetType() == FNiagaraTypeDefinition::GetVec2Def()) Value = ((const FVector2f*)Data)->ToString();
			else if (Var.GetType() == FNiagaraTypeDefinition::GetVec3Def()) Value = ((const FVector3f*)Data)->ToString();
			else if (Var.GetType() == FNiagaraTypeDefinition::GetIntDef()) Value = FString::FromInt(*(const int32*)Data);
			else if (Var.GetType() == FNiagaraTypeDefinition::GetBoolDef()) Value = (*(const int32*)Data) ? TEXT("true") : TEXT("false");
			else if (Var.GetType() == FNiagaraTypeDefinition::GetColorDef()) Value = ((const FLinearColor*)Data)->ToString();
			else if (Var.GetType() == FNiagaraTypeDefinition::GetVec4Def()) Value = ((const FVector4f*)Data)->ToString();
			else if (Var.GetType() == FNiagaraTypeDefinition::GetPositionDef()) Value = ((const FVector3f*)Data)->ToString();
			else if (Var.GetType().IsEnum()) Value = FString::Printf(TEXT("enum %d"), *(const int32*)Data);
			Out += FString::Printf(TEXT("%s | %s | %s\n"), *Script->GetName(), *Name, *Value);
		}
	}
#endif
	return Out;
}

int32 UDRVFXEditorTools::ScaleNiagaraModuleConstants(UNiagaraSystem* System, const FString& NameContains, float Factor)
{
	int32 Changed = 0;
#if WITH_EDITOR
	if (!System || NameContains.IsEmpty()) return 0;
	System->Modify();
	for (UNiagaraScript* Script : GatherScripts(System))
	{
		FNiagaraParameterStore& Store = Script->RapidIterationParameters;
		bool bScriptChanged = false;
		for (const FNiagaraVariableWithOffset& Var : Store.ReadParameterVariables())
		{
			if (!Var.GetName().ToString().Contains(NameContains)) continue;
			const uint8* Data = Store.GetParameterData(Var.Offset);
			if (Var.GetType() == FNiagaraTypeDefinition::GetFloatDef())
			{
				const float V = *(const float*)Data * Factor;
				Store.SetParameterData((const uint8*)&V, Var.Offset, sizeof(V));
			}
			else if (Var.GetType() == FNiagaraTypeDefinition::GetVec2Def())
			{
				const FVector2f V = *(const FVector2f*)Data * Factor;
				Store.SetParameterData((const uint8*)&V, Var.Offset, sizeof(V));
			}
			else if (Var.GetType() == FNiagaraTypeDefinition::GetVec3Def())
			{
				const FVector3f V = *(const FVector3f*)Data * Factor;
				Store.SetParameterData((const uint8*)&V, Var.Offset, sizeof(V));
			}
			else if (Var.GetType() == FNiagaraTypeDefinition::GetIntDef())
			{
				const int32 V = FMath::RoundToInt(*(const int32*)Data * Factor);
				Store.SetParameterData((const uint8*)&V, Var.Offset, sizeof(V));
			}
			else
			{
				continue;
			}
			if (!bScriptChanged) { Script->Modify(); bScriptChanged = true; }
			++Changed;
		}
	}
	if (Changed > 0)
	{
		System->RequestCompile(true);
		System->WaitForCompilationComplete(true, false);
		System->MarkPackageDirty();
	}
#endif
	return Changed;
}

UNiagaraSystem* UDRVFXEditorTools::CreateNiagaraSystemFromEmitter(UNiagaraEmitter* Emitter, const FString& PackagePath, const FString& AssetName)
{
#if WITH_EDITOR
	if (!Emitter || PackagePath.IsEmpty() || AssetName.IsEmpty()) return nullptr;

	const FString PackageName = PackagePath / AssetName;
	if (FindPackage(nullptr, *PackageName) || FPackageName::DoesPackageExist(PackageName)) return nullptr;

	UPackage* Package = CreatePackage(*PackageName);
	UNiagaraSystem* System = NewObject<UNiagaraSystem>(Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);

	// UNiagaraSystemFactoryNew::FactoryCreateNew 의 "이미터로 만들기" 경로와 같은 순서
	UNiagaraSystemFactoryNew::InitializeSystem(System, true);
	FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Emitter, Emitter->GetExposedVersion().VersionGuid);
	System->RequestCompile(false);
	System->WaitForCompilationComplete(true, false);

	FAssetRegistryModule::AssetCreated(System);
	Package->MarkPackageDirty();
	return System;
#else
	return nullptr;
#endif
}

FString UDRVFXEditorTools::DescribeNiagaraSystemDetailed(UNiagaraSystem* System)
{
	FString Out;
#if WITH_EDITOR
	if (!System) return TEXT("null");
	Out += FString::Printf(TEXT("SYSTEM %s warmup=%.2f fixedBounds=%d\n"), *System->GetPathName(), System->GetWarmupTime(), System->bFixedBounds ? 1 : 0);

	TArray<FNiagaraVariable> UserVariables;
	System->GetExposedParameters().GetParameters(UserVariables);
	for (const FNiagaraVariable& Variable : UserVariables)
	{
		Out += FString::Printf(TEXT("  USER %s : %s\n"), *Variable.GetName().ToString(), *Variable.GetType().GetName());
	}

	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		const FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		if (!Data) continue;
		Out += FString::Printf(TEXT("EMITTER %s enabled=%d local=%d sim=%s obj=%s\n"),
			*Handle.GetName().ToString(), Handle.GetIsEnabled() ? 1 : 0, Data->bLocalSpace ? 1 : 0,
			Data->SimTarget == ENiagaraSimTarget::GPUComputeSim ? TEXT("GPU") : TEXT("CPU"),
			*GetPathNameSafe(Handle.GetInstance().Emitter));

		int32 RendererIndex = 0;
		for (UNiagaraRendererProperties* Renderer : Data->GetRenderers())
		{
			if (!Renderer) { ++RendererIndex; continue; }
			FString Line = FString::Printf(TEXT("  R%d %s enabled=%d path=%s"), RendererIndex++,
				*Renderer->GetClass()->GetName(), Renderer->GetIsEnabled() ? 1 : 0, *Renderer->GetPathName());
			if (const UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
			{
				Line += FString::Printf(TEXT(" subUV=%gx%g align=%d facing=%d pivot=(%g,%g)"), Sprite->SubImageSize.X, Sprite->SubImageSize.Y,
					(int32)Sprite->Alignment, (int32)Sprite->FacingMode, Sprite->PivotInUVSpace.X, Sprite->PivotInUVSpace.Y);
			}
			else if (const UNiagaraMeshRendererProperties* Mesh = Cast<UNiagaraMeshRendererProperties>(Renderer))
			{
				for (const FNiagaraMeshRendererMeshProperties& MeshProps : Mesh->Meshes)
				{
					Line += TEXT(" mesh=") + GetPathNameSafe(MeshProps.Mesh);
				}
				Line += FString::Printf(TEXT(" override=%d facing=%d"), Mesh->bOverrideMaterials ? 1 : 0, (int32)Mesh->FacingMode);
			}
			else if (const UNiagaraRibbonRendererProperties* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer))
			{
				Line += FString::Printf(TEXT(" facing=%d"), (int32)Ribbon->FacingMode);
			}
			TArray<UMaterialInterface*> Materials;
			Renderer->GetUsedMaterials(nullptr, Materials);
			for (UMaterialInterface* Material : Materials)
			{
				Line += TEXT(" mat=") + MaterialSummary(Material);
			}
			Out += Line + TEXT("\n");
		}

		UNiagaraGraph* Graph = GetEmitterGraph(Data);
		if (!Graph) continue;

		TArray<UNiagaraNodeFunctionCall*> Calls;
		Graph->GetNodesOfClass(Calls);
		for (UNiagaraNodeFunctionCall* Call : Calls)
		{
			const bool bModule = Call->FunctionScript && Call->FunctionScript->IsModuleScript();
			FString Feeds;
			if (!bModule)
			{
				// 동적 입력(커브 등)이 어느 모듈 입력에 꽂혔는지 — 오버라이드 핀 이름이 "모듈.입력" 이다
				for (const UEdGraphPin* Pin : Call->Pins)
				{
					if (!Pin || Pin->Direction != EGPD_Output) continue;
					for (const UEdGraphPin* Linked : Pin->LinkedTo)
					{
						if (Linked) Feeds += TEXT(" -> ") + Linked->PinName.ToString();
					}
				}
			}
			Out += FString::Printf(TEXT("  %s [%s] %s enabled=%d%s\n"), bModule ? TEXT("M") : TEXT("  fn"),
				UsageName(FNiagaraStackGraphUtilities::GetOutputNodeUsage(*Call)), *Call->GetFunctionName(), Call->IsNodeEnabled() ? 1 : 0, *Feeds);
		}

		TArray<UObject*> Subobjects;
		GetObjectsWithOuter(Graph, Subobjects, true);
		for (UObject* Object : Subobjects)
		{
			UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(Object);
			if (!DataInterface) continue;
			FString Owner;
			if (const UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(DataInterface->GetOuter()))
			{
				Owner = InputNode->Input.GetName().ToString();
			}
			FString Line = FString::Printf(TEXT("  DI %s (%s) for=%s"), *DataInterface->GetPathName(), *DataInterface->GetClass()->GetName(), *Owner);
			for (TFieldIterator<FStructProperty> It(DataInterface->GetClass()); It; ++It)
			{
				if (It->Struct != FRichCurve::StaticStruct() || It->GetName().Contains(TEXT("Cooked"))) continue;
				Line += FString::Printf(TEXT(" %s[%s]"), *It->GetName(), *CurveKeysToString(*It->ContainerPtrToValuePtr<FRichCurve>(DataInterface)));
			}
			for (TFieldIterator<FObjectPropertyBase> It(DataInterface->GetClass()); It; ++It)
			{
				if (UObject* Referenced = It->GetObjectPropertyValue_InContainer(DataInterface))
				{
					Line += FString::Printf(TEXT(" %s=%s"), *It->GetName(), *Referenced->GetPathName());
				}
			}
			Out += Line + TEXT("\n");
		}
	}
#endif
	return Out;
}

int32 UDRVFXEditorTools::SetNiagaraModuleConstants(UNiagaraSystem* System, const FString& NameContains, const FString& ValueText)
{
	int32 Changed = 0;
#if WITH_EDITOR
	if (!System || NameContains.IsEmpty()) return 0;

	TArray<FString> Parts;
	ValueText.ParseIntoArray(Parts, TEXT(","), true);
	TArray<float> Numbers;
	for (const FString& Part : Parts)
	{
		Numbers.Add(FCString::Atof(*Part.TrimStartAndEnd()));
	}
	if (Numbers.Num() == 0) return 0;
	auto Get = [&Numbers](int32 Index) { return Numbers.IsValidIndex(Index) ? Numbers[Index] : Numbers.Last(); };

	System->Modify();
	for (UNiagaraScript* Script : GatherScripts(System))
	{
		FNiagaraParameterStore& Store = Script->RapidIterationParameters;
		bool bScriptChanged = false;
		for (const FNiagaraVariableWithOffset& Var : Store.ReadParameterVariables())
		{
			if (!Var.GetName().ToString().Contains(NameContains)) continue;
			const FNiagaraTypeDefinition& Type = Var.GetType();
			if (!bScriptChanged) { Script->Modify(); bScriptChanged = true; }

			if (Type == FNiagaraTypeDefinition::GetFloatDef())
			{
				const float V = Get(0);
				Store.SetParameterData((const uint8*)&V, Var.Offset, sizeof(V));
			}
			else if (Type == FNiagaraTypeDefinition::GetBoolDef())
			{
				const FNiagaraBool V(Get(0) != 0.0f);
				Store.SetParameterData((const uint8*)&V, Var.Offset, sizeof(V));
			}
			else if (Type == FNiagaraTypeDefinition::GetIntDef() || Type.IsEnum())
			{
				const int32 V = FMath::RoundToInt(Get(0));
				Store.SetParameterData((const uint8*)&V, Var.Offset, sizeof(V));
			}
			else if (Type == FNiagaraTypeDefinition::GetVec2Def())
			{
				const FVector2f V(Get(0), Get(1));
				Store.SetParameterData((const uint8*)&V, Var.Offset, sizeof(V));
			}
			else if (Type == FNiagaraTypeDefinition::GetVec3Def() || Type == FNiagaraTypeDefinition::GetPositionDef())
			{
				const FVector3f V(Get(0), Get(1), Get(2));
				Store.SetParameterData((const uint8*)&V, Var.Offset, sizeof(V));
			}
			else if (Type == FNiagaraTypeDefinition::GetVec4Def() || Type == FNiagaraTypeDefinition::GetQuatDef())
			{
				const FVector4f V(Get(0), Get(1), Get(2), Get(3));
				Store.SetParameterData((const uint8*)&V, Var.Offset, sizeof(V));
			}
			else if (Type == FNiagaraTypeDefinition::GetColorDef())
			{
				const FLinearColor V(Get(0), Get(1), Get(2), Numbers.Num() >= 4 ? Get(3) : 1.0f);
				Store.SetParameterData((const uint8*)&V, Var.Offset, sizeof(V));
			}
			else
			{
				continue;
			}
			UE_LOG(LogTemp, Display, TEXT("[DRVFX] %s | %s = %s"), *Script->GetName(), *Var.GetName().ToString(), *ValueText);
			++Changed;
		}
	}
	if (Changed > 0)
	{
		FinishSystemEdit(System, true);
	}
#endif
	return Changed;
}

int32 UDRVFXEditorTools::SetNiagaraModulesEnabled(UNiagaraSystem* System, const FString& EmitterName, const FString& ModuleNameContains, bool bEnabled)
{
	int32 Changed = 0;
#if WITH_EDITOR
	if (!System || ModuleNameContains.IsEmpty()) return 0;
	System->Modify();
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (!EmitterName.IsEmpty() && Handle.GetName().ToString() != EmitterName) continue;
		UNiagaraGraph* Graph = GetEmitterGraph(Handle.GetEmitterData());
		if (!Graph) continue;

		TArray<UNiagaraNodeFunctionCall*> Calls;
		Graph->GetNodesOfClass(Calls);
		for (UNiagaraNodeFunctionCall* Call : Calls)
		{
			if (!Call->FunctionScript || !Call->FunctionScript->IsModuleScript()) continue;
			if (!Call->GetFunctionName().Contains(ModuleNameContains) || Call->IsNodeEnabled() == bEnabled) continue;
			FNiagaraStackGraphUtilities::SetModuleIsEnabled(*Call, bEnabled);
			++Changed;
		}
	}
	if (Changed > 0)
	{
		FinishSystemEdit(System, true);
	}
#endif
	return Changed;
}

bool UDRVFXEditorTools::SetNiagaraCurveKeys(UObject* DataInterface, const FString& CurveName, const FString& KeysText)
{
#if WITH_EDITOR
	FStructProperty* Property = DataInterface ? FindFProperty<FStructProperty>(DataInterface->GetClass(), FName(*CurveName)) : nullptr;
	if (!Property || Property->Struct != FRichCurve::StaticStruct()) return false;

	TArray<FString> Pairs;
	KeysText.ParseIntoArray(Pairs, TEXT(";"), true);
	if (Pairs.Num() == 0) return false;

	DataInterface->Modify();
	FRichCurve* Curve = Property->ContainerPtrToValuePtr<FRichCurve>(DataInterface);
	Curve->Reset();
	for (const FString& Pair : Pairs)
	{
		// "시간:값" (선형) 또는 "시간:값:c" (부드럽게)
		TArray<FString> Fields;
		Pair.ParseIntoArray(Fields, TEXT(":"), true);
		if (Fields.Num() < 2) continue;
		const FKeyHandle Key = Curve->AddKey(FCString::Atof(*Fields[0]), FCString::Atof(*Fields[1]));
		Curve->SetKeyInterpMode(Key, Fields.Num() >= 3 && Fields[2].TrimStartAndEnd() == TEXT("c") ? RCIM_Cubic : RCIM_Linear);
	}
	Curve->AutoSetTangents();

	FPropertyChangedEvent ChangedEvent(Property, EPropertyChangeType::ValueSet);
	DataInterface->PostEditChangeProperty(ChangedEvent);
	if (UNiagaraNode* Node = Cast<UNiagaraNode>(DataInterface->GetOuter()))
	{
		Node->MarkNodeRequiresSynchronization(TEXT("DRVFX curve keys"), true);
	}
	DataInterface->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

void UDRVFXEditorTools::RecompileNiagaraSystem(UNiagaraSystem* System, bool bMarkGraphsChanged)
{
#if WITH_EDITOR
	if (!System) return;
	System->Modify();
	if (bMarkGraphsChanged)
	{
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			if (UEdGraph* Graph = GetEmitterGraph(Handle.GetEmitterData()))
			{
				Graph->NotifyGraphChanged();
			}
		}
	}
	FinishSystemEdit(System, true);
#endif
}

bool UDRVFXEditorTools::CopyNiagaraEmitter(UNiagaraSystem* Source, const FString& SourceEmitterName, UNiagaraSystem* Target, const FString& NewName)
{
#if WITH_EDITOR
	if (!Source || !Target) return false;
	FVersionedNiagaraEmitter Instance;
	for (const FNiagaraEmitterHandle& Handle : Source->GetEmitterHandles())
	{
		if (Handle.GetName().ToString() == SourceEmitterName)
		{
			Instance = Handle.GetInstance();
			break;
		}
	}
	if (!Instance.Emitter) return false;

	Target->Modify();
	const FGuid NewId = FNiagaraEditorUtilities::AddEmitterToSystem(*Target, *Instance.Emitter, Instance.Version);
	for (FNiagaraEmitterHandle& Handle : Target->GetEmitterHandles())
	{
		if (Handle.GetId() == NewId && !NewName.IsEmpty())
		{
			Handle.SetName(FName(*NewName), *Target);
			break;
		}
	}
	FinishSystemEdit(Target, false);
	return true;
#else
	return false;
#endif
}

bool UDRVFXEditorTools::AddNiagaraEmitterAsset(UNiagaraSystem* Target, UNiagaraEmitter* EmitterAsset, const FString& NewName)
{
#if WITH_EDITOR
	if (!Target || !EmitterAsset) return false;
	Target->Modify();
	const FGuid NewId = FNiagaraEditorUtilities::AddEmitterToSystem(*Target, *EmitterAsset, EmitterAsset->GetExposedVersion().VersionGuid);
	for (FNiagaraEmitterHandle& Handle : Target->GetEmitterHandles())
	{
		if (Handle.GetId() == NewId && !NewName.IsEmpty())
		{
			Handle.SetName(FName(*NewName), *Target);
			break;
		}
	}
	FinishSystemEdit(Target, false);
	return true;
#else
	return false;
#endif
}

bool UDRVFXEditorTools::RemoveNiagaraEmitter(UNiagaraSystem* System, const FString& EmitterName)
{
#if WITH_EDITOR
	if (!System) return false;
	TSet<FGuid> ToRemove;
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (Handle.GetName().ToString() == EmitterName)
		{
			ToRemove.Add(Handle.GetId());
		}
	}
	if (ToRemove.Num() == 0) return false;
	System->Modify();
	System->RemoveEmitterHandlesById(ToRemove);
	FinishSystemEdit(System, false);
	return true;
#else
	return false;
#endif
}

bool UDRVFXEditorTools::SetNiagaraEmitterGPU(UNiagaraSystem* System, const FString& EmitterName, bool bGPU)
{
#if WITH_EDITOR
	if (!System) return false;
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (Handle.GetName().ToString() != EmitterName) continue;
		FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		if (!Data) return false;
		System->Modify();
		if (UNiagaraEmitter* Emitter = Handle.GetInstance().Emitter)
		{
			Emitter->Modify();
		}
		Data->SimTarget = bGPU ? ENiagaraSimTarget::GPUComputeSim : ENiagaraSimTarget::CPUSim;
		// GPU 는 고정 경계가 필요하다 — 없으면 동적으로 잡게 둔다
		FinishSystemEdit(System, true);
		return true;
	}
#endif
	return false;
}

int32 UDRVFXEditorTools::FinishShaderCompilation()
{
#if WITH_EDITOR
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
		return GShaderCompilingManager->GetNumRemainingJobs();
	}
#endif
	return 0;
}
