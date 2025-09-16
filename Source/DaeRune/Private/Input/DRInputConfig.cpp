// Copyright DaeRune


#include "Input/DRInputConfig.h"

const UInputAction* UDRInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	// 입력 액션 배열에서 일치하는 태그 검색
	for (const FDRInputAction& Action : AbilityInputActions)
	{
		// InputAction이 유효하고 태그가 일치하는지 확인
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction;
		}
	}

	// 일치하는 액션이 없으면 nullptr 반환
	return nullptr;
}
