// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AttributeInfo.generated.h"

// 속성 정보 구조체 선언문  
USTRUCT(BlueprintType)
struct FDRAttributeInfo
{
	GENERATED_BODY()

	// 속성 태그 변수  
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag AttributeTag = FGameplayTag();

	// 속성 이름 변수  
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText AttributeName = FText();

	// 속성 설명 변수 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText AttributeDescription = FText();

	// 속성 값 변수
	UPROPERTY(BlueprintReadOnly)
	float AttributeValue = 0.f;
};

/**
 * 속성 정보 데이터에셋 클래스 선언문
 */
UCLASS()
class DAERUNE_API UAttributeInfo : public UDataAsset
{
	GENERATED_BODY()
	
public:
	// 태그로 속성 정보 조회 함수 선언문  
	FDRAttributeInfo FindAttributeInfoForTag(const FGameplayTag& AttributeTag, bool bLogNotFound = false) const;

	// 속성 정보 배열 변수  
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FDRAttributeInfo> AttributeInformation;
};
