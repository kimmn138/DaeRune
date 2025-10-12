// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "DRInputConfig.h"
#include "DRInputComponent.generated.h"

/**
 * Enhanced Input System을 확장한 DaeRune 전용 입력 컴포넌트
 */
UCLASS()
class DAERUNE_API UDRInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()
	
public:
	// 어빌리티 액션 바인딩 템플릿 함수
	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
	void BindAbilityActions(const UDRInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, HeldFuncType HeldFunc);
};

template <class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
void UDRInputComponent::BindAbilityActions(const UDRInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, HeldFuncType HeldFunc)
{
	// InputConfig 유효성 검증
	check(InputConfig);

	// 설정된 모든 어빌리티 입력 액션 순회
	for (const FDRInputAction& Action : InputConfig->AbilityInputActions)
	{
		// InputAction과 GameplayTag가 모두 유효한지 확인
		if (Action.InputAction && Action.InputTag.IsValid())
		{
			// 입력 시작 이벤트 바인딩 (키를 누르는 순간)
			if (PressedFunc)
			{
				BindAction(Action.InputAction, ETriggerEvent::Started, Object, PressedFunc, Action.InputTag);
			}

			// 입력 해제 이벤트 바인딩 (키를 떼는 순간)
			if (ReleasedFunc)
			{
				BindAction(Action.InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, Action.InputTag);
			}

			// 입력 지속 이벤트 바인딩 (키를 누르고 있는 동안 매 프레임)
			if (HeldFunc)
			{
				BindAction(Action.InputAction, ETriggerEvent::Triggered, Object, HeldFunc, Action.InputTag);
			}
		}
	}
}