// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterClassInfo.generated.h"

class UGameplayEffect;
class UGameplayAbility;

// 플레이어 캐릭터 클래스 열거타입
UENUM(BlueprintType)
enum class EPlayerCharacterClass : uint8
{
	GardenRobot,
	CleanRobot,
	VendRobot
};

// 적 캐릭터 클래스 열거타입
UENUM(BlueprintType)
enum class EEnemyCharacterClass : uint8
{
	Elementalist,
	Warrior,
	Ranger
};

// 캐릭터 클래스 기본 정보 구조체
USTRUCT(BlueprintType)
struct FCharacterClassDefaultInfo
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
	TSubclassOf<UGameplayEffect> VitalAttributes; // 기본 속성 효과 클래스

	UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities; // 초기 능력 배열
};

/**
 * 캐릭터 클래스 정보 데이터에셋 클래스
 */
UCLASS()
class DAERUNE_API UCharacterClassInfo : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
	TMap<EPlayerCharacterClass, FCharacterClassDefaultInfo> PlayerCharacterClassInformation; // 플레이어 클래스 정보 맵

	UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
	TMap<EEnemyCharacterClass, FCharacterClassDefaultInfo> EnemyCharacterClassInformation; // 적 클래스 정보 맵

	UPROPERTY(EditDefaultsOnly, Category = "Common Class Defaults")
	TArray<TSubclassOf<UGameplayAbility>> CommonAbilities;

	// 플레이어 클래스 기본 정보 조회 함수
	const FCharacterClassDefaultInfo& GetPlayerClassDefaultInfo(EPlayerCharacterClass CharacterClass);

	// 적 클래스 기본 정보 조회 함수
	const FCharacterClassDefaultInfo& GetEnemyClassDefaultInfo(EEnemyCharacterClass CharacterClass);
};
