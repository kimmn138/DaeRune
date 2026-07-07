// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterClassInfo.generated.h"

class UGameplayEffect;
class UGameplayAbility;
class ADRCharacter;

// UDRUserWidget forward declaration (TSubclassOf only needs forward decl in UE5.5 with UHT)
class UDRUserWidget;
class UUserWidget;

UENUM(BlueprintType)
enum class ECharacterClass : uint8
{
	Elementalist,
	Warrior,
	Ranger,
	Bear,
	PartEnemy
};

UENUM(BlueprintType)
enum class EPlayerCharacterClass : uint8
{
	Gardener,
	VendingMachine,

	Count UMETA(Hidden) // 클래스 개수 계산용 - 항상 마지막에 유지
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

	UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
	TArray<TSubclassOf<UGameplayAbility>> DeathAbilities;

	// 해당 캐릭터 전용 스킬아이콘 위젯 클래스
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UDRUserWidget> SkillIconWidgetClass;

	// Tab 키로 띄우는 캐릭터 설명창 위젯 클래스
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> CharacterInfoWidgetClass;
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
	TMap<ECharacterClass, FCharacterClassDefaultInfo> CharacterClassInformation;

	UPROPERTY(EditDefaultsOnly, Category = "Common Class Defaults")
	TArray<TSubclassOf<UGameplayAbility>> CommonAbilities;

	FCharacterClassDefaultInfo GetClassDefaultInfo(ECharacterClass CharacterClass);
};

/**
 * 플레이어 전용 캐릭터 클래스 정보 데이터 에셋
 */
UCLASS()
class DAERUNE_API UPlayerCharacterClassInfo : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
	TMap<EPlayerCharacterClass, FCharacterClassDefaultInfo> CharacterClassInformation;

	UPROPERTY(EditDefaultsOnly, Category = "Common Class Defaults")
	TArray<TSubclassOf<UGameplayAbility>> CommonAbilities;

	// 클래스별 캐릭터 BP 매핑 (외형은 BP에서 전부 설정)
	UPROPERTY(EditDefaultsOnly, Category = "Character Blueprint")
	TMap<EPlayerCharacterClass, TSubclassOf<ADRCharacter>> CharacterBPClasses;

	FCharacterClassDefaultInfo GetClassDefaultInfo(EPlayerCharacterClass CharacterClass);
};
