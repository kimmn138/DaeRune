// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DRPhaseBase.generated.h"

class ADRStageGameMode;
class ADRStageGameState;
class ADRCleanserSite;

USTRUCT(BlueprintType)
struct FPhaseObjectiveData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PhaseNumber = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText ObjectiveTitle; // "클렌저를 확보하세요"

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText ProgressFormat; // "확보한 클렌저"

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequiredCount = 0;
};

/**
 * 모든 페이즈의 기본 클래스
 * 각 페이즈의 공통 로직을 담당
 */
UCLASS(Blueprintable, Abstract)
class DAERUNE_API UDRPhaseBase : public UObject
{
	GENERATED_BODY()
	
public:
	// ========== 페이즈 생명주기 ==========

	// 페이즈 초기화 (GameMode가 생성 후 호출)
	UFUNCTION(BlueprintCallable, Category = "Phase")
	virtual void Initialize(ADRStageGameMode* InGameMode, ADRStageGameState* InGameState);

	// 페이즈 시작 (GameMode가 호출)
	UFUNCTION(BlueprintCallable, Category = "Phase")
	virtual void OnPhaseStart();

	// 페이즈 종료 (정리 작업)
	UFUNCTION(BlueprintCallable, Category = "Phase")
	virtual void OnPhaseEnd();

	// ========== 클렌저 사이트 관리 ==========

	// 전체 클렌저 사이트 설정 (GameMode가 초기화 시 호출)
	UFUNCTION(BlueprintCallable, Category = "Phase")
	void SetCleanserSites(const TArray<ADRCleanserSite*>& InCleanserSites);

	// 전체 클렌저 사이트 가져오기
	const TArray<TObjectPtr<ADRCleanserSite>>& GetCleanserSites() const { return CleanserSites; }

	// Phase1에서 선택된 활성 클렌저 사이트 설정 (2개)
	void SetActiveCleanserSites(const TArray<TObjectPtr<ADRCleanserSite>>& InActiveSites);

	// Phase1에서 선택된 활성 클렌저 사이트 가져오기 (Phase2, 3에서 사용)
	const TArray<TObjectPtr<ADRCleanserSite>>& GetActiveCleanserSites() const { return ActiveCleanserSites; }

	// ========== 적 관리 ==========

	// 적이 죽었을 때 델리게이트로 호출됨
	UFUNCTION()
	virtual void OnEnemyDeath(AActor* DeadEnemy);

protected:
	// ========== 헬퍼 함수 ==========

	// 살아있는 적 수 체크
	UFUNCTION(BlueprintCallable, Category = "Phase")
	int32 GetAliveEnemyCount() const;

	// ========== 변수 ==========

	// 페이즈가 시작되었는지 여부
	UPROPERTY(BlueprintReadOnly, Category = "Phase")
	bool bIsPhaseActive;

	// Phase1 시작 시점의 플레이어 수
	UPROPERTY(BlueprintReadOnly, Category = "Phase")
	int32 InitialPlayerCount;

	// 스폰된 적들
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SpawnedEnemies;

	// GameMode 참조
	UPROPERTY()
	TObjectPtr<ADRStageGameMode> GameMode;

	// GameState 참조
	UPROPERTY()
	TObjectPtr<ADRStageGameState> GameState;

	// 레벨에 배치된 전체 클렌저 사이트 3개 (모든 페이즈가 공유)
	UPROPERTY(BlueprintReadOnly, Category = "Phase")
	TArray<TObjectPtr<ADRCleanserSite>> CleanserSites;

	// Phase1에서 선택된 활성 클렌저 사이트 2개
	// Phase2, 3에서도 이 사이트들을 사용
	UPROPERTY(BlueprintReadOnly, Category = "Phase")
	TArray<TObjectPtr<ADRCleanserSite>> ActiveCleanserSites;

	// ========== 페이즈 목표 UI ==========
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Config")
	UDataTable* PhaseObjectiveDataTable;
	
	void SetupPhaseObjective(int32 PhaseNumber);
};
