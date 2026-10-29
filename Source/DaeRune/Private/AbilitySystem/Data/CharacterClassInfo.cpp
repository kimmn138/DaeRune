// Copyright DaeRune


#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DaeRune/DRLogChannels.h"

namespace
{
	template <typename EnumType>
	const FCharacterClassDefaultInfo& FindClassInfoOrEmpty(const TMap<EnumType, FCharacterClassDefaultInfo>& Table,
		EnumType CharacterClass, const UObject* Owner)
	{
		if (const FCharacterClassDefaultInfo* Found = Table.Find(CharacterClass))
		{
			return *Found;
		}

		UE_LOG(LogDR, Warning, TEXT("[ClassInfo] %s 에 클래스 %d 항목이 없습니다 — 빈 기본값을 씁니다."),
			*GetNameSafe(Owner), static_cast<int32>(CharacterClass));
		static const FCharacterClassDefaultInfo Empty;
		return Empty;
	}
}

const FCharacterClassDefaultInfo& UCharacterClassInfo::GetClassDefaultInfo(ECharacterClass CharacterClass) const
{
	return FindClassInfoOrEmpty(CharacterClassInformation, CharacterClass, this);
}

const FCharacterClassDefaultInfo& UPlayerCharacterClassInfo::GetClassDefaultInfo(EPlayerCharacterClass CharacterClass) const
{
	return FindClassInfoOrEmpty(CharacterClassInformation, CharacterClass, this);
}
