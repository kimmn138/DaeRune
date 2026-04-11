// Copyright DaeRune


#include "AbilitySystem/Data/CharacterClassInfo.h"

FCharacterClassDefaultInfo UCharacterClassInfo::GetClassDefaultInfo(ECharacterClass CharacterClass)
{
	return CharacterClassInformation.FindChecked(CharacterClass);
}

FCharacterClassDefaultInfo UPlayerCharacterClassInfo::GetClassDefaultInfo(EPlayerCharacterClass CharacterClass)
{
	return CharacterClassInformation.FindChecked(CharacterClass);
}
