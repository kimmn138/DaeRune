// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameModeBase.h"
#include "DRTutorialGameMode.generated.h"

class ADRCharacter;

/**
 * 튜토리얼 전용 게임 모드
 * 튜토리얼 클리어 시 SaveGame에 완료 플래그를 저장하고 메인 메뉴로 복귀한다.
 */
UCLASS()
class DAERUNE_API ADRTutorialGameMode : public ADRGameModeBase
{
	GENERATED_BODY()

public:
	ADRTutorialGameMode();

	// 플레이어별 DefaultPawnClass 결정
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	// 튜토리얼 완료 트리거 (블루프린트에서 호출 가능)
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void TriggerTutorialComplete();

protected:
	virtual void BeginPlay() override;
	virtual void HandleWipeout() override;

	// 메인 메뉴 맵 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	FString MainMenuMapName = TEXT("MainMenu");

	// 튜토리얼 완료 후 복귀 딜레이
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
	float TutorialCompleteDelay = 3.0f;

private:
	void ReturnToMainMenu();

	// 모든 플레이어에게 튜토리얼 완료 알림
	void NotifyAllPlayersTutorialComplete();

	FTimerHandle TutorialCompleteTimerHandle;
};
