// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRGameModeBase.generated.h"

class UAbilityInfo;
class UCharacterClassInfo;

/**
 * DaeRune 기본 게임 모드 클래스
 */
UCLASS()
class DAERUNE_API ADRGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	// 캐릭터 클래스별 기본 설정
	UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
	TObjectPtr<UCharacterClassInfo> CharacterClassInfo;

	// 어빌리티 정보 데이터 에셋
	UPROPERTY(EditDefaultsOnly, Category = "Ability Info")
	TObjectPtr<UAbilityInfo> AbilityInfo;
};
