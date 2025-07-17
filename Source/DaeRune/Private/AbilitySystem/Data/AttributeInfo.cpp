// Copyright DaeRune


#include "AbilitySystem/Data/AttributeInfo.h"
#include "DaeRune/DRLogChannels.h"

// 태그로 속성 정보 조회 함수 정의문  
FDRAttributeInfo UAttributeInfo::FindAttributeInfoForTag(const FGameplayTag& AttributeTag, bool bLogNotFound) const
{
	// 속성 정보 배열 순회문 
	for (const FDRAttributeInfo& Info : AttributeInformation)
	{
		// 태그 일치 여부 확인문
		if (Info.AttributeTag.MatchesTagExact(AttributeTag))
		{
			// 일치 정보 반환문
			return Info;
		}
	}

	// 로그 출력 여부 확인문 
	if (bLogNotFound)
	{
		// 오류 로그 출력문
		UE_LOG(LogDR, Error, TEXT("Can't find Info for AttributeTag [%s] on AttributeInfo [%s]."), *AttributeTag.ToString(), *GetNameSafe(this));
	}

	// 기본 정보 반환문  
	return FDRAttributeInfo();
}
