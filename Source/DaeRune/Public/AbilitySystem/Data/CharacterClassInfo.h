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
class UTexture2D;

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
	RobotVacuum,

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

	// 캐릭터 전용 기본 조준선 텍스처
	UPROPERTY(EditDefaultsOnly, Category = "UI|Crosshair")
	TObjectPtr<UTexture2D> CrosshairTexture;

	// 부품 운반 중 사이트 조준 시(설치 가능) 캐릭터 전용 조준선 텍스처
	UPROPERTY(EditDefaultsOnly, Category = "UI|Crosshair")
	TObjectPtr<UTexture2D> CrosshairInstallReadyTexture;

	// 캐릭터 전용 조준선 표시 크기 (0,0 이면 위젯에서 텍스처 원본 크기 사용)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Crosshair")
	FVector2D CrosshairSize = FVector2D(32.f, 32.f);
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

	// 부품 운반 중(공격 불가) 조준선 텍스처 - 모든 플레이어 캐릭터 공통
	UPROPERTY(EditDefaultsOnly, Category = "Common Class Defaults")
	TObjectPtr<UTexture2D> CrosshairDisabledTexture;

	FCharacterClassDefaultInfo GetClassDefaultInfo(EPlayerCharacterClass CharacterClass);
};
