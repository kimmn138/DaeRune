// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DRGameplayAbility.generated.h"

/**
 * 커스텀 GameplayAbility 클래스 선언
 */
UCLASS()
class DAERUNE_API UDRGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	// 입력 바인딩용 태그 프로퍼티
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag StartupInputTag;
};
