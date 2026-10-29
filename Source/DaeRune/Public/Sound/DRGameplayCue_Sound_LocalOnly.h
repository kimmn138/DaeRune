// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Sound/DRGameplayCue_Sound.h"
#include "GameFramework/Pawn.h"
#include "DRGameplayCue_Sound_LocalOnly.generated.h"

/**
 * 로컬 전용 사운드 GameplayCue — 대상이 이 머신에서 조종하는 폰일 때만 재생
 */
UCLASS()
class DAERUNE_API UDRGameplayCue_Sound_LocalOnly : public UDRGameplayCue_Sound
{
	GENERATED_BODY()

protected:
	virtual bool ShouldPlayFor(const AActor* MyTarget) const override
	{
		const APawn* Pawn = Cast<APawn>(MyTarget);
		return Pawn && Pawn->IsLocallyControlled();
	}
};
