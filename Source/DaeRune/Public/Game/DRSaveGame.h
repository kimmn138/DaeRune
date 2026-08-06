// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRProgressionTypes.h"
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

	// 캐릭터(클래스)별 진행도. 키가 없으면 {Level=1, XP=0}으로 간주(로드 시 lazy 초기화).
	UPROPERTY(VisibleAnywhere, Category = "Progress")
	TMap<EPlayerCharacterClass, FDRCharacterProgress> CharacterProgress;

	// 세이브 포맷 버전 (추후 XP 곡선/스탯 재조정 시 마이그레이션 분기점).
	UPROPERTY(VisibleAnywhere, Category = "Progress")
	int32 SaveVersion = 1;

	// 저장 슬롯 이름
	static const FString SaveSlotName;
	static const int32 UserIndex;

	// 현재 코드가 기대하는 세이브 포맷 버전 (마이그레이션 비교 기준).
	static constexpr int32 CurrentSaveVersion = 1;
};
