// Copyright DaeRune

#include "Editor/DRVFXEditorTools.h"

#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraSystem.h"

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
