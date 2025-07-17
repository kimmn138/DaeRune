// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRGameModeBase.generated.h"

class UAbilityInfo;
class UCharacterClassInfo;

/**
 * ADRGameModeBase
 *
 * 게임 모드 기본 설정 클래스 정의문서
 */
UCLASS()
class DAERUNE_API ADRGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	/**
	 * CharacterClassInfo
	 *
	 * 기본 캐릭터 클래스 설정용 데이터 에셋 참조임
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
	TObjectPtr<UCharacterClassInfo> CharacterClassInfo;

	/**
	 * AbilityInfo
	 *
	 * 능력 정보 설정용 데이터 에셋 참조임
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Ability Info")
	TObjectPtr<UAbilityInfo> AbilityInfo;
};
