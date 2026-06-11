// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Phase/DRPhaseBase.h"
#include "DRPhase1.generated.h"

class ADRCleanserSite;
class ADRDoorManager;
class ADREnemy;
class ADREnemySpawnGroup;

/**
 * New Phase1: 클렌저 확보 + 부품 회수 통합
 *  - 맵에 배치된 클렌저 사이트 1개를 활성화
 *  - 사이트 주변에 EnemiesToSpawn 배열로 명시된 적들을 배열 길이만큼 스폰
 *  - 통로(DREnemySpawnGroup)에 미리 배치된 적들 중 그룹별로 아르마딜로 제외 후보 1마리를 무작위로 부품 운반자로 지정
 *  - 모든 Phase1 적에게 State.Enemy.Phase1 태그를 부여하여 ExecCalc_Damage에서 0.7배 곱
 *  - 클렌저 사이트에 부품 2개(RequiredPartsCount, 사이트 BP 기본값) 설치 시 페이즈 완료
 */
UCLASS(Blueprintable)
class DAERUNE_API UDRPhase1 : public UDRPhaseBase
{
	GENERATED_BODY()

public:
	virtual void OnPhaseStart() override;
	virtual void OnPhaseEnd() override;
	virtual void OnEnemyDeath(AActor* DeadEnemy) override;

protected:
	// ========== 사이트 1개 강제 ==========
	void KeepSingleCleanserSite();

	// ========== 사이트 주변 스폰 ==========
	void SpawnEnemiesAroundCleanserSite(ADRCleanserSite* Site);
	AActor* SpawnEnemy(TSubclassOf<AActor> EnemyClass, const FVector& Location);

	// ========== 통로 스폰 그룹 / 부품 운반자 ==========
	void CollectSpawnGroups();
	void AssignPartCarriersForAllGroups();
	ADREnemy* PickPartCarrierFromGroup(ADREnemySpawnGroup* Group) const;
	void ConfigurePartCarrier(ADREnemy* Carrier) const;

	// ========== Phase1 태그 부여 ==========
	void ApplyPhase1Tag(ADREnemy* Enemy) const;

	// ========== 부품 설치 이벤트 ==========
	UFUNCTION()
	void OnPartInstalled(ADRCleanserSite* Site);

	// ========== 도어 매니저 ==========
	ADRDoorManager* GetDoorManager();

protected:
	// ========== 사이트 주변 스폰 구성 ==========
	// 클렌저 사이트의 Phase1EnemySpawnOffsets[i] 위치에 EnemiesToSpawn[i] 클래스의 적을 1마리 스폰
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase1|Spawn")
	TArray<TSubclassOf<ADREnemy>> EnemiesToSpawn;

	// ========== 부품 운반자 선정 ==========
	// 부품 휴대 대상에서 제외할 적 클래스(아르마딜로)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase1|Parts")
	TSubclassOf<ADREnemy> ArmadilloEnemyClass;

	// 부품 운반자에게 세팅할 부품 액터 클래스 (DREnemy::PartActorClass와 동일)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase1|Parts")
	TSubclassOf<AActor> PartActorClass;

	// ========== 대미지 감소 태그 ==========
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase1|Combat")
	FGameplayTag Phase1EnemyTag;

private:
	// 활성화된 단일 클렌저 사이트 캐시
	UPROPERTY()
	TWeakObjectPtr<ADRCleanserSite> ActiveSite;

	// 월드에서 자동 수집한 통로 스폰 그룹
	UPROPERTY()
	TArray<TObjectPtr<ADREnemySpawnGroup>> SpawnGroups;

	// 그룹별 부품 운반자
	UPROPERTY()
	TMap<TObjectPtr<ADREnemySpawnGroup>, TWeakObjectPtr<ADREnemy>> PartCarrierByGroup;

	// 캐싱된 DoorManager 참조
	UPROPERTY()
	TObjectPtr<ADRDoorManager> CachedDoorManager;
};
