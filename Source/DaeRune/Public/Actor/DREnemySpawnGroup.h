// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DREnemySpawnGroup.generated.h"

class ADREnemy;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllEnemiesDead);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDied, ADREnemy*, DeadEnemy);

UCLASS()
class DAERUNE_API ADREnemySpawnGroup : public AActor
{
	GENERATED_BODY()
	
public:
	ADREnemySpawnGroup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 외부에서 호출할 함수

	// 그룹에 적 등록
	UFUNCTION(BlueprintCallable, Category = "SpawnGroup")
	void RegisterEnemy(ADREnemy* Enemy);

	// 그룹의 모든 적 활성화
	UFUNCTION(BlueprintCallable, Category = "SpawnGroup")
	void ActivateAllEnemies();

	// 남은 적 수 반환
	UFUNCTION(BlueprintPure, Category = "SpawnGroup")
	int32 GetAliveEnemyCount() const { return AliveEnemyCount; }

	// 전멸 여부
	UFUNCTION(BlueprintPure, Category = "SpawnGroup")
	bool AreAllEnemiesDead() const { return bAllEnemiesDead; }

	// 등록된 살아있는 적 목록을 반환 (Phase1의 부품 운반자 후보 추리기에 사용)
	UFUNCTION(BlueprintCallable, Category = "SpawnGroup")
	TArray<ADREnemy*> GetRegisteredEnemies() const;

	// 부품 운반자 캐시 (서버에서만 의미 있음; 디버깅/UI 연동용)
	UFUNCTION(BlueprintPure, Category = "SpawnGroup")
	ADREnemy* GetPartCarrierEnemy() const { return PartCarrierEnemy.Get(); }

	void SetPartCarrierEnemy(ADREnemy* Carrier);

	// Delegates
	UPROPERTY(BlueprintAssignable, Category = "SpawnGroup")
	FOnAllEnemiesDead OnAllEnemiesDead;

	UPROPERTY(BlueprintAssignable, Category = "SpawnGroup")
	FOnEnemyDied OnEnemyDied;

protected:
	virtual void BeginPlay() override;

	// 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	// 미리 배치된 적 참조
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnGroup|PrePlaced")
	TArray<TSoftObjectPtr<ADREnemy>> PrePlacedEnemies;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<ADREnemy>> RegisteredEnemies;

	// 서버 권위: Phase1이 선정한 부품 운반자
	TWeakObjectPtr<ADREnemy> PartCarrierEnemy;

	UPROPERTY(Replicated)
	int32 AliveEnemyCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_AllEnemiesDead)
	bool bAllEnemiesDead = false;

	UFUNCTION()
	void OnRep_AllEnemiesDead();

	UFUNCTION()
	void OnEnemyDestroyed(AActor* DestroyedActor);

	void CheckAllEnemiesDead();

};
