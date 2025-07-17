// Copyright DaeRune


#include "AbilitySystem/Data/AbilityInfo.h"
#include "DaeRune/DRLogChannels.h"

// 태그 기반 어빌리티 정보 조회 함수 정의문  
FDRAbilityInfo UAbilityInfo::FindAbilityInfoForTag(const FGameplayTag& AbilityTag, bool bLogNotFound) const
{
	// 정보 배열 순회문  
	for (const FDRAbilityInfo& Info : AbilityInformation)
	{
		// 태그 일치 여부 검사문
		if (Info.AbilityTag == AbilityTag)
		{
			// 일치 정보 반환문
			return Info;
		}
	}

	// 로그 출력 조건 검사문  
	if (bLogNotFound)
	{
		// 오류 로그 출력문
		UE_LOG(LogDR, Error, TEXT("Can't find info for AbilityTag [%s] on AbilityInfo [%s]"), *AbilityTag.ToString(), *GetNameSafe(this));
	}

	// 기본 구조체 반환문
	return FDRAbilityInfo();
}
