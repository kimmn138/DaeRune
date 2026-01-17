// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "StatusEffectInfo.generated.h"

USTRUCT(BlueprintType)
struct FEffectInfo
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag EffectTag = FGameplayTag();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText EffectName = FText();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UTexture2D> EffectIcon = nullptr;

	UPROPERTY(BlueprintReadOnly)
	bool bIsDebuff = false;
    
	UPROPERTY(BlueprintReadOnly)
	bool bHasDuration = true;
    
	UPROPERTY(BlueprintReadOnly)
	float Duration = 0.f;
    
	UPROPERTY(BlueprintReadOnly)
	bool bDisplayStack = false;

	UPROPERTY(BlueprintReadOnly)
	int32 StackCount = 0;
};

/**
 * 
 */
UCLASS()
class DAERUNE_API UStatusEffectInfo : public UDataAsset
{
	GENERATED_BODY()

public:
	FEffectInfo FindEffectInfoForTag(const FGameplayTag& EffectTag, bool bLogNotFound = false) const;
    
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FEffectInfo> EffectsInformation;
};
