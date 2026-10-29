// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"

class FProperty;
class UScriptStruct;

/**
 * 튜닝 값을 리플렉션으로 읽고 쓰는 도우미 (UDRTuningSubsystem · 에디터 반영 도구 공용).
 *
 * 속성 경로 문법 — 점(.)으로 내려가고 대괄호로 배열 · 맵 · 표의 줄을 고른다.
 *   WaterCost                                   CDO 의 숫자 속성
 *   Damage.Value                                구조체 안 (FScalableFloat)
 *   SlotCosts[2]                                배열 칸 번호
 *   Chips[CM.Stat.MaxHealth].Modifiers[0].Value  구조체 배열을 Id 로 (ChipId · StageId 처럼 이름이 …Id 인 FName 속성)
 *   CharacterMovement.JumpZVelocity             CDO 의 기본 서브오브젝트(컴포넌트 템플릿) 안
 *   BTDecorator_Blackboard_0.FloatValue         첫 칸이 속성이 아니면 그 이름의 서브오브젝트 (비헤이비어 트리 노드)
 *   [Wave1].PlayDuration                        데이터 테이블 줄
 *   [Abilities.MoleBoss.Claw].Keys[1].Value     커브 테이블 줄의 키
 * 대괄호 안의 점은 나누지 않는다 (칩 Id 에 점이 들어 있다).
 */
namespace DRTuning
{
	/** 경로를 따라 찾은 값 1칸 */
	struct FResolvedValue
	{
		FProperty* Property = nullptr;		// 값의 속성 (배열 원소면 Inner)
		void* ValuePtr = nullptr;			// 값이 있는 메모리
		UObject* OwnerObject = nullptr;		// 값을 품은 UObject (서브오브젝트면 그 서브오브젝트)

		bool IsValid() const { return Property != nullptr && ValuePtr != nullptr; }
	};

	/** 원래 값을 보관 · 비교하기 위한 값 사본 (숫자는 double/int64, 참거짓은 bool, 나머지는 글자) */
	struct DAERUNE_API FTypedValue
	{
		enum class EKind : uint8 { None, Float, Int, Bool, Text };

		EKind Kind = EKind::None;
		double Float = 0.0;
		int64 Int = 0;
		bool bBool = false;
		FString Text;

		bool IsSet() const { return Kind != EKind::None; }
		bool operator==(const FTypedValue& Other) const;
		bool operator!=(const FTypedValue& Other) const { return !(*this == Other); }
	};

	/** Root(CDO · 데이터 에셋 · 데이터/커브 테이블)에서 Path 를 찾는다. 못 찾으면 false + OutError(한국어) */
	DAERUNE_API bool ResolvePath(UObject* Root, const FString& Path, FResolvedValue& Out, FString& OutError);

	/** 값 → 시트 글자 (0.2 / 25 / true / Percent) */
	DAERUNE_API FString ValueToText(const FResolvedValue& Value);

	/**
	 * 시트 글자 → 값. 숫자 · 참거짓 · 열거형은 직접 해석하고(10% → 0.1, 1,200 → 1200, TRUE/참),
	 * 그 밖의 타입은 엔진 ImportText 로 넘긴다. 실패하면 값은 그대로이고 false + OutError.
	 */
	DAERUNE_API bool SetValueFromText(const FResolvedValue& Value, const FString& Text, FString& OutError);

	DAERUNE_API FTypedValue ReadValue(const FResolvedValue& Value);
	DAERUNE_API void WriteValue(const FResolvedValue& Value, const FTypedValue& NewValue);

	/** 시트에 "튜닝 값" 으로 싣는 잎 속성인가 — 숫자 · 참거짓만 (열거형은 바꿀 수는 있지만 목록에는 싣지 않는다) */
	DAERUNE_API bool IsListableLeaf(const FProperty* Property);

	/** 숫자 속성이면 double 로 (범위 검사용). 숫자가 아니면 false */
	DAERUNE_API bool TryGetNumber(const FResolvedValue& Value, double& OutNumber);

	/**
	 * 에셋 경로 → 값을 덮어쓸 오브젝트. BP 면 그 클래스의 CDO, 나머지(데이터 에셋 · 테이블)는 에셋 그 자체.
	 * /Game/X/GA_Y.GA_Y_C · /Game/X/GA_Y.GA_Y · /Game/X/GA_Y 모두 받는다 (쿠킹 빌드에는 UBlueprint 가 없어 _C 를 붙여 다시 찾는다).
	 * OutLoadPath 에는 다음에 같은 것을 다시 찾을 때 쓸 정규 경로(BP 는 _C)를 돌려준다.
	 */
	DAERUNE_API UObject* LoadTargetObject(const FString& ObjectPath, FString& OutLoadPath, FString& OutError);

	/** 구조체 배열 원소를 이름으로 고를 때 쓰는 열쇠 속성 — 이름이 Id 로 끝나는 FName/FString 속성. 없으면 null */
	DAERUNE_API FProperty* FindKeyProperty(const UScriptStruct* Struct);

	/** 배열 원소의 Id · 맵 열쇠를 [ ] 안에 쓰는 글자로 (이름 · 문자열 · 열거형 짧은 이름 · 태그) */
	DAERUNE_API FString KeyToText(const FProperty* KeyProperty, const void* KeyPtr);

	/**
	 * 배치 액터 대상인가 — 맵 안 액터의 경로(/Game/Maps/Stage1.Stage1:PersistentLevel.BP_Door_C_1).
	 * 맞으면 맵 패키지(/Game/Maps/Stage1)와 액터 이름을 돌려준다. 이런 대상은 맵을 불러오지 않고 그 맵이 열릴 때 적용한다.
	 */
	DAERUNE_API bool ParseLevelActorPath(const FString& ObjectPath, FName& OutMapPackage, FName& OutActorName);

	/** 속성 이름 — 블루프린트 구조체(UserDefinedStruct)는 GUID 가 붙은 내부 이름 대신 작성자가 지은 이름 */
	DAERUNE_API FString GetPropertyAuthoredName(const FProperty* Property);
}
