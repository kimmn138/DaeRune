// Copyright DaeRune


#include "AbilitySystem/Data/StatusEffectInfo.h"

FEffectInfo UStatusEffectInfo::FindEffectInfoForTag(const FGameplayTag& EffectTag, bool bLogNotFound) const
{
	for (FEffectInfo Info : EffectsInformation)
	{
		if (Info.EffectTag.MatchesTagExact(EffectTag))
		{
			return Info;
		}
	}

	return FEffectInfo();
}
