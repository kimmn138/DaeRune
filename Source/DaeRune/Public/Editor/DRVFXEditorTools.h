// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRVFXEditorTools.generated.h"

class UNiagaraEmitter;
class UNiagaraSystem;

/**
 * 나이아가라 에셋 점검 · 수정 도우미 — ★에디터 전용★ (게임 빌드에서는 본문이 비어 있다).
 * Python 은 이미터 설정(로컬 공간 등)을 읽고 쓸 수 없어서 C++ 로 둔다. 저장은 호출한 쪽이 한다.
 *   desc = unreal.DRVFXEditorTools.describe_niagara_system(sys)
 *   n = unreal.DRVFXEditorTools.set_niagara_emitters_local_space(sys, True)
 */
UCLASS()
class DAERUNE_API UDRVFXEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 이미터마다 이름 · 켜짐 · 로컬 공간 · CPU/GPU · 렌더러 종류를 한 줄씩 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static FString DescribeNiagaraSystem(UNiagaraSystem* System);

	/**
	 * 모든 이미터의 Local Space 를 바꾸고 다시 컴파일한다 (바뀐 이미터 수를 돌려준다).
	 * 로컬 공간이면 입자가 컴포넌트를 따라 움직이고 컴포넌트 스케일도 입자 위치 · 크기에 먹는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static int32 SetNiagaraEmittersLocalSpace(UNiagaraSystem* System, bool bLocalSpace);

	/** 이미터 · 시스템 스크립트의 모듈 상수(스택에서 입력한 값)를 "스크립트 | 이름 | 값" 으로. NameFilter 가 비면 전부 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static FString DumpNiagaraModuleConstants(UNiagaraSystem* System, const FString& NameFilter);

	/**
	 * 이름에 NameContains 가 들어간 float / Vector2 / Vector3 모듈 상수에 Factor 를 곱하고 다시 컴파일한다 (바꾼 값 수).
	 * 예) 스프라이트 크기 줄이기 — 나이아가라 스프라이트 크기는 컴포넌트 스케일을 따르지 않아서 에셋 값을 바꿔야 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static int32 ScaleNiagaraModuleConstants(UNiagaraSystem* System, const FString& NameContains, float Factor);

	/**
	 * 이미터(NE) 하나를 담은 새 시스템(NS) 에셋을 만든다 — 에디터의 "이미터로 시스템 만들기"와 같다 (이미터는 복사되어 들어간다).
	 * PackagePath 예) "/Game/Basic_VFX/Niagara/Dash". 같은 이름이 이미 있으면 null. 저장은 호출한 쪽이 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static UNiagaraSystem* CreateNiagaraSystemFromEmitter(UNiagaraEmitter* Emitter, const FString& PackagePath, const FString& AssetName);
};
