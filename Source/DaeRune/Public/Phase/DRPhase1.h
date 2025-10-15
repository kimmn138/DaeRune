// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Phase/DRPhaseBase.h"
#include "DRPhase1.generated.h"

class ADRCleanserSite;

/**
 * Phase 1: 클렌저 확보
 * - 3개 클렌저 지점 중 2곳 랜덤 선택
 * - 각 지점에 강력한 몬스터 1마리 + 일반 몬스터 4마리 스폰
 * - 모든 적 처치시 완료
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
	// ========== 스폰 함수 ==========

	// 클렌저 사이트 선택 및 적 스폰
	UFUNCTION(BlueprintCallable, Category = "Phase1")
	void SpawnEnemiesAtCleanserSites();

	// 특정 클렌저 사이트에 적 그룹 스폰
	UFUNCTION(BlueprintCallable, Category = "Phase1")
	void SpawnEnemyGroupAtCleanserSite(ADRCleanserSite* CleanserSite);

	// 적 스폰 헬퍼
	UFUNCTION(BlueprintCallable, Category = "Phase1")
	AActor* SpawnEnemy(TSubclassOf<AActor> EnemyClass, const FVector& Location);

	// ========== 설정 변수 ==========

	// 강력한 몬스터 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase1|Config")
	TSubclassOf<AActor> EliteEnemyClass;

	// 일반 몬스터 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase1|Config")
	TSubclassOf<AActor> NormalEnemyClass;

	// 맵에 배치된 클렌저 사이트 3개 (블루프린트에서 할당)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Phase1|Config", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<ADRCleanserSite>> AllCleanserSites;

	// 일반 몬스터 스폰 수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase1|Config")
	int32 NormalEnemyCount = 4;

	// 몬스터 스폰 반경 (클렌저 주변)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase1|Config")
	float SpawnRadius = 200.0f;

	// 엘리트 몬스터 오프셋 (클렌저 중심에서 떨어뜨릴 거리)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase1|Config")
	float EliteSpawnOffset = 100.0f;

private:
	// 이번 Phase에서 선택된 클렌저 사이트 2개
	UPROPERTY()
	TArray<TObjectPtr<ADRCleanserSite>> SelectedCleanserSites;

	// 총 스폰된 적 수
	int32 TotalEnemyCount = 0;
};
