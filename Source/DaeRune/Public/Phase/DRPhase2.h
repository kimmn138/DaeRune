// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Phase/DRPhaseBase.h"
#include "DRPhase2.generated.h"

class ADRCleanserSite;
class ADRCleanserPart;
class ADREnemy;

/**
 * Phase 2: 부품 수집 및 설치
 * - 맵의 4곳에서 부품을 들고 도망치는 적 스폰
 * - 플레이어가 부품을 수집하여 클렌저 사이트에 설치
 * - 각 클렌저 사이트에 2개씩 총 4개 설치 시 완료
 */
UCLASS(Blueprintable)
class DAERUNE_API UDRPhase2 : public UDRPhaseBase
{
	GENERATED_BODY()
	
public:
	virtual void OnPhaseStart() override;
	virtual void OnPhaseEnd() override;

protected:
	// ========== 스폰 함수 ==========

	// 스폰 포인트 찾기
	UFUNCTION(BlueprintCallable, Category = "Phase2")
	void FindEnemySpawnPoints();

	// 부품을 들고 도망치는 적 스폰
	UFUNCTION(BlueprintCallable, Category = "Phase2")
	void SpawnPartCarryingEnemies();

	// 특정 위치에 적 스폰
	UFUNCTION(BlueprintCallable, Category = "Phase2")
	ADREnemy* SpawnEnemyAtLocation(AActor* SpawnPoint);

	// ========== 이벤트 처리 ==========

	// 부품 설치 완료 이벤트
	UFUNCTION()
	void OnPartInstalled(ADRCleanserSite* Site);

	// 페이즈 완료 조건 체크
	UFUNCTION(BlueprintCallable, Category = "Phase2")
	void CheckPhaseCompletion();

	// ========== 설정 변수 ==========

	// 부품을 들고 도망치는 적 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase2|Config")
	TSubclassOf<ADREnemy> PartCarryingEnemyClass;

	// 스폰 포인트를 찾을 태그 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase2|Config")
	FName SpawnPointTag = "Phase2SpawnPoint";

	// 적 스폰 위치 액터들 (4개)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Phase2|Config")
	TArray<TObjectPtr<AActor>> EnemySpawnPoints;

private:
	// 설치 완료된 클렌저 사이트 추적
	UPROPERTY()
	TSet<TObjectPtr<ADRCleanserSite>> CompletedSites;
};
