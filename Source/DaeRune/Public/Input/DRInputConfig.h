// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DRInputConfig.generated.h"

// Enhanced Input Action과 GameplayTag를 연결하는 구조체
USTRUCT(BlueprintType)
struct FDRInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	const class UInputAction* InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly)
	FGameplayTag InputTag = FGameplayTag();
};

/**
 * 어빌리티 입력 설정을 관리하는 데이터 에셋
 */
UCLASS()
class DAERUNE_API UDRInputConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:
	// 특정 GameplayTag에 대응하는 InputAction 검색
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = false) const;

	// 어빌리티 입력 액션 배열
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FDRInputAction> AbilityInputActions;
};
