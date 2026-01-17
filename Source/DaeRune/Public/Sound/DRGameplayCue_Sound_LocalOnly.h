// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Sound/DRGameplayCue_Sound.h"
#include "DRGameplayCue_Sound_LocalOnly.generated.h"

/**
 * 로컬 전용 사운드 GameplayCue
 */
UCLASS()
class DAERUNE_API UDRGameplayCue_Sound_LocalOnly : public UDRGameplayCue_Sound
{
	GENERATED_BODY()
	
public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
