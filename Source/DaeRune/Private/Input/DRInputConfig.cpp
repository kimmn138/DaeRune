// Copyright DaeRune


#include "Input/DRInputConfig.h"

const UInputAction* UDRInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const // 입력 액션 검색 함수 구현임
{
	// 등록된 입력 액션 리스트 순회임
	for (const FDRInputAction& Action : AbilityInputActions)
	{
		// 액션 유효성 및 태그 일치 검사임
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction; // 매칭 입력 액션 반환임
		}
	}

	if (bLogNotFound)
	{
		// 미발견 시 에러 로깅임
		UE_LOG(LogTemp, Error, TEXT("Can't find AbilityInputAction for InputTag [%s], on InputConfig [%s]"), *InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr; // 검색 실패 시 nullptr 반환임
}
