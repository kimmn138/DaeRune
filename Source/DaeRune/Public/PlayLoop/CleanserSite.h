// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CleanserSite.generated.h"

class AEnemyDummy;

UENUM(BluePrintType)
enum class ECleanserState : uint8 { Flag, Cylinder};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSiteCleared, class ACleanserSite*, Site);

UCLASS()
class DAERUNE_API ACleanserSite : public AActor
{
	GENERATED_BODY()
	
public:	
	ACleanserSite();

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, Category = "Cleanser");
    UStaticMesh* FlagMesh;

    UPROPERTY(EditAnywhere, Category = "Cleanser")
    UStaticMesh* CylinderMesh;

    UPROPERTY(EditAnywhere, Category = "Cleanser")
    TSubclassOf<AEnemyDummy> EnemyClass;

    UPROPERTY(EditAnywhere, Category = "Cleanser")
    int32 NumEnemiesToSpawn = 4;

    UPROPERTY(EditAnywhere, Category = "Cleanser")
    float SpawnRadius = 300.f;

    UPROPERTY(BlueprintAssignable)
    FOnSiteCleared OnSiteCleared;

    UFUNCTION(BlueprintCallable)
    void InitializeAndSpawn();     // 스폰 직후 호출

    UFUNCTION(BlueprintCallable)
    void ForceKillAll();           // 디버그용


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:
    ECleanserState State = ECleanserState::Flag;

    UPROPERTY()
    TArray<AEnemyDummy*> Spawned;

    void SetState(ECleanserState NewState);
    void SpawnEnemiesAround();

    UFUNCTION()
    void OnEnemyDiedHandler(AActor* Enemy);

    void TryClear();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
