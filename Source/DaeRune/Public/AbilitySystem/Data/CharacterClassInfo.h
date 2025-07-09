// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterClassInfo.generated.h"

class UGameplayEffect;
class UGameplayAbility;

UENUM(BlueprintType)
enum class EPlayerCharacterClass : uint8
{
	GardenRobot,
	CleanRobot,
	VendRobot
};

UENUM(BlueprintType)
enum class EEnemyCharacterClass : uint8
{
	Elementalist,
	Warrior,
	Ranger
};

USTRUCT(BlueprintType)
struct FCharacterClassDefaultInfo
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
	TSubclassOf<UGameplayEffect> PrimaryAttributes;

	UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
	TSubclassOf<UGameplayEffect> VitalAttributes;

	UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;
};

/**
 * 
 */
UCLASS()
class DAERUNE_API UCharacterClassInfo : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
	TMap<EPlayerCharacterClass, FCharacterClassDefaultInfo> PlayerCharacterClassInformation;

	UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
	TMap<EEnemyCharacterClass, FCharacterClassDefaultInfo> EnemyCharacterClassInformation;

	UPROPERTY(EditDefaultsOnly, Category = "Common Class Defaults")
	TArray<TSubclassOf<UGameplayAbility>> CommonAbilities;

	const FCharacterClassDefaultInfo& GetPlayerClassDefaultInfo(EPlayerCharacterClass CharacterClass);

	const FCharacterClassDefaultInfo& GetEnemyClassDefaultInfo(EEnemyCharacterClass CharacterClass);
};
