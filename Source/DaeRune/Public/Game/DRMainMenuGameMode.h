// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRMainMenuGameMode.generated.h"

UCLASS()
class DAERUNE_API ADRMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADRMainMenuGameMode();

protected:
	virtual void BeginPlay() override;

	// 튜토리얼 완료 여부 확인 (향후 SaveGame 연동)
	bool HasCompletedTutorial() const;
};
