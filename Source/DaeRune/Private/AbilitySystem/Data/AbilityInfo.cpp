// Copyright DaeRune


#include "AbilitySystem/Data/AbilityInfo.h"
#include "DaeRune/DRLogChannels.h"

FDRAbilityInfo UAbilityInfo::FindAbilityInfoForTag(const FGameplayTag& AbilityTag, bool bLogNotFound) const
{
	for (const FDRAbilityInfo& Info : AbilityInformation)
	{
		if (Info.AbilityTag == AbilityTag)
		{
			return Info;
		}
	}

		if (bLogNotFound)
		{
			UE_LOG(LogDR, Error, TEXT("Can't find info for AbilityTag [%s] on AbilityInfo [%s]"), *AbilityTag.ToString(), *GetNameSafe(this));
		}

	return FDRAbilityInfo();
}
