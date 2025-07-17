// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DRInputConfig.generated.h"

/**
 * FDRInputAction
 *
 * 입력 액션과 대응 태그 매핑용 구조체 정의문서
 */
USTRUCT(BlueprintType)
struct FDRInputAction // 입력 액션-태그 페어 구조체임
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	const class UInputAction* InputAction = nullptr; // 입력 액션 참조임

	UPROPERTY(EditDefaultsOnly)
	FGameplayTag InputTag = FGameplayTag(); // 입력 태그 식별자임
};

/**
 * UDRInputConfig
 *
 * 어빌리티 입력 액션 매핑 데이터 에셋 클래스임
 */
UCLASS()
class DAERUNE_API UDRInputConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:
	/**
	 * FindAbilityInputActionForTag
	 *
	 * 태그에 해당하는 입력 액션 검색 함수 선언임
	 */
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = false) const;

	// 에디터에서 설정 가능한 어빌리티 입력 액션 배열임
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FDRInputAction> AbilityInputActions;
};
