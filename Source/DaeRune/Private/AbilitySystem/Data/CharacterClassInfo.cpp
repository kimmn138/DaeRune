// Copyright DaeRune


#include "AbilitySystem/Data/CharacterClassInfo.h"

// 플레이어 클래스 기본 정보 조회 함수 정의
const FCharacterClassDefaultInfo& UCharacterClassInfo::GetPlayerClassDefaultInfo(EPlayerCharacterClass CharacterClass)
{
	return PlayerCharacterClassInformation.FindChecked(CharacterClass); // 맵 조회
}

// 적 클래스 기본 정보 조회 함수 정의
const FCharacterClassDefaultInfo& UCharacterClassInfo::GetEnemyClassDefaultInfo(EEnemyCharacterClass CharacterClass)
{
	return EnemyCharacterClassInformation.FindChecked(CharacterClass); // 맵 조회
}
