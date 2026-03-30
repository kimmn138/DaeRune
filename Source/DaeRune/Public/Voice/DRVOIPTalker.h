// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Net/VoiceConfig.h"
#include "DRVOIPTalker.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRVOIPTalker : public UVOIPTalker
{
	GENERATED_BODY()
	
protected:
	virtual void OnTalkingBegin(UAudioComponent* AudioComponent) override;
};
