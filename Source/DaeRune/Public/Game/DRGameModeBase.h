// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRGameModeBase.generated.h"

class UAbilityInfo;
class UCharacterClassInfo;

/**
 * 
 */
UCLASS()
class DAERUNE_API ADRGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
	TObjectPtr<UCharacterClassInfo> CharacterClassInfo;

	UPROPERTY(EditDefaultsOnly, Category = "Ability Info")
	TObjectPtr<UAbilityInfo> AbilityInfo;
};
