// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DRSaveGame.generated.h"

/**
 * 플레이어 진행도를 저장하는 SaveGame 클래스
 */
UCLASS()
class DAERUNE_API UDRSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UDRSaveGame();

	// 튜토리얼 완료 플래그
	UPROPERTY(VisibleAnywhere, Category = "Progress")
	bool bHasCompletedTutorial = false;

	// 저장 슬롯 이름
	static const FString SaveSlotName;
	static const int32 UserIndex;
};
