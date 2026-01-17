// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRDoorManager.generated.h"

class ADRAutoSlidingDoor;
class ADRBreakableDoor;
class ADREnemySpawnGroup;

UCLASS()
class DAERUNE_API ADRDoorManager : public AActor
{
	GENERATED_BODY()
	
public:
	ADRDoorManager();

	// Phase에서 호출할 함수
	UFUNCTION(BlueprintCallable, Category = "DoorManager")
	void OnPhase1Ended();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	// 자동문 참조
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DoorManager|AutoDoors")
	TObjectPtr<ADRAutoSlidingDoor> AutoDoor_Room1ToRoom6;

	// 부서지는 문 참조
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DoorManager|BreakableDoors")
	TObjectPtr<ADRBreakableDoor> BreakableDoor_Room1ToRoom2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DoorManager|BreakableDoors")
	TObjectPtr<ADRBreakableDoor> BreakableDoor_Room1ToRoom3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DoorManager|BreakableDoors")
	TObjectPtr<ADRBreakableDoor> BreakableDoor_Room4ToRoom6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DoorManager|BreakableDoors")
	TObjectPtr<ADRBreakableDoor> BreakableDoor_Room5ToRoom6;

	// 스폰 그룹 참조
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DoorManager|SpawnGroups")
	TObjectPtr<ADREnemySpawnGroup> SpawnGroup_Room4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DoorManager|SpawnGroups")
	TObjectPtr<ADREnemySpawnGroup> SpawnGroup_Room5;

	// 타이밍 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DoorManager|Timing")
	float DelayAfterAutoDoorClosed = 0.5f;

private:
	UFUNCTION()
	void OnAutoDoorClosed();

	UFUNCTION()
	void OnRoom4Cleared();

	UFUNCTION()
	void OnRoom5Cleared();

	void BreakInitialDoors();
	void BreakRemainingDoors();
	void BindDoorEvents();
	void BindSpawnGroupEvents();

	FTimerHandle InitialBreakTimerHandle;

	bool bInitialDoorsTriggered = false;

};
