// Copyright DaeRune


#include "AbilitySystem/Data/StatusEffectInfo.h"

FEffectInfo UStatusEffectInfo::FindEffectInfoForTag(const FGameplayTag& EffectTag) const
{
	// 원소마다 복사하지 않고 찾은 것만 돌려준다 (FText 포함 구조체)
	const FEffectInfo* Found = EffectsInformation.FindByPredicate(
		[&EffectTag](const FEffectInfo& Info) { return Info.EffectTag.MatchesTagExact(EffectTag); });
	return Found ? *Found : FEffectInfo();
}
