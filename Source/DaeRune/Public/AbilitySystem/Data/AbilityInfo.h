// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AbilityInfo.generated.h"

class UGameplayAbility;

// 능력 정보 구조체 선언문  
USTRUCT(BlueprintType)
struct FDRAbilityInfo
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag AbilityTag = FGameplayTag(); // 능력 태그 변수

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag InputTag = FGameplayTag(); // 입력 태그 변수

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag CooldownTag = FGameplayTag(); // 쿨다운 태그 변수

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag AbilityType = FGameplayTag(); // 능력 타입 태그 변수

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UTexture2D> Icon = nullptr; // 아이콘 텍스처 변수

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UMaterialInterface> BackgroundMaterial = nullptr; // 배경 머티리얼 변수

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> Ability; // 어빌리티 클래스 변수

};

/**
 * 어빌리티 정보 데이터에셋 클래스 선언문
 */
UCLASS()
class DAERUNE_API UAbilityInfo : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityInformation")
	TArray<FDRAbilityInfo> AbilityInformation; // 어빌리티 정보 배열 변수

	// 태그 기반 조회 함수 선언문
	FDRAbilityInfo FindAbilityInfoForTag(const FGameplayTag& AbilityTag, bool bLogNotFound = false) const;
};
