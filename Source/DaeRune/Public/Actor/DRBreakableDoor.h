// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRBreakableDoor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDoorBroken);

class ADREnemy;
class UNiagaraSystem;
class ANavLinkProxy;

UCLASS()
class DAERUNE_API ADRBreakableDoor : public AActor
{
	GENERATED_BODY()
	
public:
	ADRBreakableDoor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 외부에서 호출할 함수
	UFUNCTION(BlueprintCallable, Category = "Door")
	void Break();

	UFUNCTION(BlueprintCallable, Category = "Door")
	void BreakWithoutEnemies();

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsBroken() const { return bIsBroken; }

	// Delegate
	UPROPERTY(BlueprintAssignable, Category = "Door")
	FOnDoorBroken OnDoorBroken;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	// 문이 부서지면 활성화할 Nav Link
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Navigation")
	TObjectPtr<ANavLinkProxy> NavLinkProxy;

	// 물리 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Physics")
	FVector BreakDirection = FVector(1.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Physics", meta = (ClampMin = "100.0", ClampMax = "5000.0"))
	float ImpulseStrength = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SidewaysSpreadRatio = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UpwardForceRatio = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RandomVariation = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Physics", meta = (ClampMin = "0.1", ClampMax = "30.0"))
	float PieceLifetime = 5.0f;

	// 사운드 & 이펙트
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Effects")
	TObjectPtr<USoundBase> BreakSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Effects")
	TObjectPtr<UParticleSystem> BreakParticle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Effects")
	TObjectPtr<UNiagaraSystem> BreakNiagara;

	// 몬스터 스폰 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Enemies")
	bool bSpawnEnemiesOnBreak = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Enemies", meta = (EditCondition = "bSpawnEnemiesOnBreak"))
	TSubclassOf<ADREnemy> EnemyClassToSpawn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Enemies", meta = (EditCondition = "bSpawnEnemiesOnBreak"))
	int32 EnemySpawnCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Enemies", meta = (EditCondition = "bSpawnEnemiesOnBreak", MakeEditWidget = true))
	TArray<FVector> EnemySpawnOffsets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Enemies", meta = (EditCondition = "bSpawnEnemiesOnBreak"))
	float EnemyLaunchStrength = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Enemies", meta = (EditCondition = "bSpawnEnemiesOnBreak"))
	float EnemyLaunchUpwardRatio = 0.1f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_IsBroken)
	bool bIsBroken = false;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> DoorPieces;

	struct FPieceInitialTransform
	{
		FVector Location;
		FRotator Rotation;
	};
	TArray<FPieceInitialTransform> InitialTransforms;

	FTimerHandle CleanupTimerHandle;
	bool bShouldSpawnEnemies = true;

	UFUNCTION()
	void OnRep_IsBroken();

	void CollectDoorPieces();
	void ExecuteBreak();
	void ApplyImpulseToPieces();
	FVector CalculatePieceImpulse(UStaticMeshComponent* Piece, int32 PieceIndex);
	void SpawnEnemies();
	void CleanupPieces();
	void PlayBreakEffects();

	// Multicast RPC
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayBreakEffect();

};
