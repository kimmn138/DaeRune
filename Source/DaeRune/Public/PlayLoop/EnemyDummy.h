// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyDummy.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDied, AActor*, Enemy);

UCLASS()
class DAERUNE_API AEnemyDummy : public AActor
{
	GENERATED_BODY()
	
public:	
	AEnemyDummy();

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Mesh;

	UPROPERTY(BlueprintAssignable)
	FOnEnemyDied OnEnemyDied;

	UFUNCTION(BlueprintCallable)
	void Die();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()	
	void OnClicked_UFN(UPrimitiveComponent* Touched, FKey Button);

public:	
	virtual void Tick(float DeltaTime) override;
};
