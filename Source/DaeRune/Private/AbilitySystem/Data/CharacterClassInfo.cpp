// Copyright DaeRune


#include "AbilitySystem/Data/CharacterClassInfo.h"

const FCharacterClassDefaultInfo& UCharacterClassInfo::GetPlayerClassDefaultInfo(EPlayerCharacterClass CharacterClass)
{
	return PlayerCharacterClassInformation.FindChecked(CharacterClass);
}

const FCharacterClassDefaultInfo& UCharacterClassInfo::GetEnemyClassDefaultInfo(EEnemyCharacterClass CharacterClass)
{
	return EnemyCharacterClassInformation.FindChecked(CharacterClass);
}
