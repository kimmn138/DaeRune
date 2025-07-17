// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "DRInputConfig.h"
#include "DRInputComponent.generated.h"

/**
 * UDRInputComponent
 *
 * Enhanced Input 기반 어빌리티 액션 바인딩 컴포넌트 클래스임
 */
UCLASS()
class DAERUNE_API UDRInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()
	
public:
	/**
	 * BindAbilityActions
	 *
	 * InputConfig 기반 어빌리티 액션 바인딩 기능임
	 */
	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
	void BindAbilityActions(const UDRInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, HeldFuncType HeldFunc);
};

// BindAbilityActions 구현부
template <class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
void UDRInputComponent::BindAbilityActions(const UDRInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, HeldFuncType HeldFunc)
{
	check(InputConfig); // InputConfig 유효성 검사 기능임

	// AbilityInputActions 리스트 순회 기능임
	for (const FDRInputAction& Action : InputConfig->AbilityInputActions)
	{
		// 액션 및 태그 유효성 검사 기능임
		if (Action.InputAction && Action.InputTag.IsValid())
		{
			// 입력 시작 이벤트 바인딩 기능임
			if (PressedFunc)
			{
				BindAction(Action.InputAction, ETriggerEvent::Started, Object, PressedFunc, Action.InputTag);
			}

			// 입력 종료 이벤트 바인딩 기능임
			if (ReleasedFunc)
			{
				BindAction(Action.InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, Action.InputTag);
			}

			// 입력 유지 이벤트 바인딩 기능임
			if (HeldFunc)
			{
				BindAction(Action.InputAction, ETriggerEvent::Triggered, Object, HeldFunc, Action.InputTag);
			}
		}
	}
}