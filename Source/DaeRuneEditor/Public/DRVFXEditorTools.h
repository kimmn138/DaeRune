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
class DAERUNEEDITOR_API UDRVFXEditorTools : public UBlueprintFunctionLibrary
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

	/** 이름이 EmitterName 인 이미터를 켜거나 끄고 다시 컴파일한다 (그런 이미터가 없으면 false) */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool SetNiagaraEmitterEnabled(UNiagaraSystem* System, const FString& EmitterName, bool bEnabled);

	/** 이미터 · 시스템 스크립트의 모듈 상수(스택에서 입력한 값)를 "스크립트 | 이름 | 값" 으로. NameFilter 가 비면 전부 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static FString DumpNiagaraModuleConstants(UNiagaraSystem* System, const FString& NameFilter);

	/**
	 * 이름에 NameContains 가 들어간 float / Vector2 / Vector3 / int(반올림 — 예: Spawn Count) 모듈 상수에 Factor 를 곱하고 다시 컴파일한다 (바꾼 값 수).
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

	// ---- 이펙트 화풍 통일(2026-11-20) 도구: 점검 → Python 이 렌더러 · 커브 오브젝트를 직접 고침 → 다시 컴파일 ----

	/**
	 * DescribeNiagaraSystem 의 자세한 판 — 이미터 오브젝트 경로, 렌더러(경로 · 머티리얼 · 블렌드 · 서브 UV · 메시),
	 * 모듈(용도 · 켜짐), 데이터 인터페이스(경로 · 커브 키), 사용자 파라미터. 렌더러 · DI 경로는 Python find_object 로 잡아 고친다.
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static FString DescribeNiagaraSystemDetailed(UNiagaraSystem* System);

	/**
	 * 이름에 NameContains 가 들어간 모듈 상수를 ValueText 로 바꾸고 다시 컴파일한다 (바꾼 값 수).
	 * ValueText = 쉼표로 나눈 숫자: float "1.5" · int "3" · bool "1" · Vector2 "1,2" · Vector3/Position "1,2,3" · Vector4/Color "1,0.5,0,1".
	 * 이름은 "Constants.<이미터>.<모듈>.<입력>" 꼴이라 이미터 · 모듈까지 넣어 좁힌다.
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static int32 SetNiagaraModuleConstants(UNiagaraSystem* System, const FString& NameContains, const FString& ValueText);

	/** 이미터 EmitterName(빈 문자열 = 전부)에서 이름에 ModuleNameContains 가 들어간 모듈을 켜거나 끄고 다시 컴파일한다 (바꾼 모듈 수) */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static int32 SetNiagaraModulesEnabled(UNiagaraSystem* System, const FString& EmitterName, const FString& ModuleNameContains, bool bEnabled);

	/**
	 * DI 의 커브(이름 예: "Curve" · "RedCurve" · "AlphaCurve" · "XCurve")를 "시간:값;시간:값" 키로 바꾼다 (선형 보간).
	 * 그래프 노드에 변경을 알려 다음 컴파일이 캐시를 쓰지 않게 한다 — 끝나면 RecompileNiagaraSystem.
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool SetNiagaraCurveKeys(UObject* DataInterface, const FString& CurveName, const FString& KeysText);

	/** Python 이 렌더러 · DI 를 직접 고친 뒤 부른다 — 강제 재컴파일 + 대기 + dirty. bMarkGraphsChanged 면 이미터 그래프를 바뀐 것으로 표시 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static void RecompileNiagaraSystem(UNiagaraSystem* System, bool bMarkGraphsChanged);

	/** Source 시스템의 이미터를 복사해 Target 에 NewName 으로 넣는다 (Source 가 Target 이어도 된다 = 이미터 복제) */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool CopyNiagaraEmitter(UNiagaraSystem* Source, const FString& SourceEmitterName, UNiagaraSystem* Target, const FString& NewName);

	/** 이미터 에셋(NE · 엔진 템플릿)을 복사해 Target 에 NewName 으로 넣는다 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool AddNiagaraEmitterAsset(UNiagaraSystem* Target, UNiagaraEmitter* EmitterAsset, const FString& NewName);

	/** 이름이 EmitterName 인 이미터를 시스템에서 지운다 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool RemoveNiagaraEmitter(UNiagaraSystem* System, const FString& EmitterName);

	/** 이미터의 시뮬레이션을 CPU/GPU 로 바꾸고 다시 컴파일한다 (그런 이미터가 없으면 false) */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool SetNiagaraEmitterGPU(UNiagaraSystem* System, const FString& EmitterName, bool bGPU);

	/**
	 * 밀린 셰이더 · 에셋 컴파일을 모두 끝낸다 (남은 셰이더 작업 수, 0 이면 완료).
	 * 새 머티리얼 인스턴스는 셰이더가 백그라운드로 컴파일되는 동안 그려지지 않아 미리보기 캡처 전에 부른다.
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static int32 FinishShaderCompilation();
};
