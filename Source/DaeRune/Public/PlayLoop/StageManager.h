// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StageManager.generated.h"

class ACleanserSite;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPhase1Completed);

UCLASS()
class DAERUNE_API AStageManager : public AActor
{
	GENERATED_BODY()
	
public:	
    AStageManager();

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<ACleanserSite> CleanserSiteClass;

    UPROPERTY(EditAnywhere, Category = "Spawn")
    FName SpawnPointTag = "CleanserSpawn";

    UPROPERTY(BlueprintAssignable)
    FOnPhase1Completed OnPhase1Completed;

    UPROPERTY(BlueprintReadOnly)
    ACleanserSite* ActiveSite = nullptr;

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable)
    void KillAllEnemies(); // 디버그 버튼용
	// Sets default values for this actor's properties

private:
    UFUNCTION()
    void OnSiteCleared(ACleanserSite* Site);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};