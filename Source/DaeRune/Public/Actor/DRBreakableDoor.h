// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRBreakableDoor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDoorBroken);

class ADREnemy;
class UNiagaraSystem;
class UNavModifierComponent;

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

	// 문이 NavMesh를 막는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNavModifierComponent> NavModifierComponent;

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

	// 적을 벽 뒤에 먼저 스폰하고 이 시간(초) 뒤에 벽을 부순다 — 원격 클라에도 적이 먼저 복제돼 있게. 그동안 적 AI 는 멈춰 둔다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Enemies", meta = (EditCondition = "bSpawnEnemiesOnBreak", ClampMin = "0.0", ClampMax = "2.0"))
	float EnemySpawnLeadTime = 0.3f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_IsBroken)
	bool bIsBroken = false;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> DoorPieces;

	FTimerHandle CleanupTimerHandle;

	// 적을 스폰해 두고 벽이 부서지기를 기다리는 중 (서버) — 그 사이 Break 재호출은 무시
	bool bBreakPending = false;
	FTimerHandle BreakDelayTimerHandle;

	// 벽 뒤에서 기다리는 적 — 벽이 부서질 때 AI 를 다시 켜고 BreakDirection 으로 날린다
	TArray<TWeakObjectPtr<ADREnemy>> WaitingEnemies;

	// 이 머신에서 파편 연출(물리 · 이펙트)을 이미 재생했는지 — 늦게 들어온 클라만 OnRep 에서 숨김 처리
	bool bBreakEffectPlayed = false;

	// 파편을 날리고 이펙트를 재생한다 (머신당 1회)
	void PlayBreakSequence();

	UFUNCTION()
	void OnRep_IsBroken();

	void CollectDoorPieces();
	void ExecuteBreak();
	void ApplyImpulseToPieces();
	FVector CalculatePieceImpulse(UStaticMeshComponent* Piece, int32 PieceIndex);
	void SpawnEnemies();
	void ReleaseWaitingEnemies();
	void CleanupPieces();
	void PlayBreakEffects();

	// Multicast RPC
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayBreakEffect();

};
