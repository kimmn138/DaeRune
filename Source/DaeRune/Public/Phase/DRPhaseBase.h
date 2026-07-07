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
	FText ObjectiveTitle; // "Ŭ������ Ȯ���ϼ���"

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText ProgressFormat; // "Ȯ���� Ŭ����"

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequiredCount = 0;
};

/**
 * ��� �������� �⺻ Ŭ����
 * �� �������� ���� ������ ���
 */
UCLASS(Blueprintable, Abstract)
class DAERUNE_API UDRPhaseBase : public UObject
{
	GENERATED_BODY()
	
public:
	// ========== ������ �����ֱ� ==========

	// ������ �ʱ�ȭ (GameMode�� ���� �� ȣ��)
	UFUNCTION(BlueprintCallable, Category = "Phase")
	virtual void Initialize(ADRStageGameMode* InGameMode, ADRStageGameState* InGameState);

	// ������ ���� (GameMode�� ȣ��)
	UFUNCTION(BlueprintCallable, Category = "Phase")
	virtual void OnPhaseStart();

	// ������ ���� (���� �۾�)
	UFUNCTION(BlueprintCallable, Category = "Phase")
	virtual void OnPhaseEnd();

	// 이 페이즈의 완료 조건 충족 여부 (파생 클래스가 구현)
	// GameMode의 인덱스 switch 대신 각 페이즈가 스스로 완료를 판정한다
	virtual bool IsCompleted() const { return false; }

	// ========== Ŭ���� ����Ʈ ���� ==========

	// ��ü Ŭ���� ����Ʈ ���� (GameMode�� �ʱ�ȭ �� ȣ��)
	UFUNCTION(BlueprintCallable, Category = "Phase")
	void SetCleanserSites(const TArray<ADRCleanserSite*>& InCleanserSites);

	// ��ü Ŭ���� ����Ʈ ��������
	const TArray<TObjectPtr<ADRCleanserSite>>& GetCleanserSites() const { return CleanserSites; }

	// Phase1���� ���õ� Ȱ�� Ŭ���� ����Ʈ ���� (2��)
	void SetActiveCleanserSites(const TArray<TObjectPtr<ADRCleanserSite>>& InActiveSites);

	// Phase1���� ���õ� Ȱ�� Ŭ���� ����Ʈ �������� (Phase2, 3���� ���)
	const TArray<TObjectPtr<ADRCleanserSite>>& GetActiveCleanserSites() const { return ActiveCleanserSites; }

	// ========== �� ���� ==========

	// ���� �׾��� �� ��������Ʈ�� ȣ���
	UFUNCTION()
	virtual void OnEnemyDeath(AActor* DeadEnemy);

protected:
	// ========== ���� �Լ� ==========

	// ����ִ� �� �� üũ
	UFUNCTION(BlueprintCallable, Category = "Phase")
	int32 GetAliveEnemyCount() const;

	// ========== ���� ==========

	// ����� ���۵Ǿ����� ����
	UPROPERTY(BlueprintReadOnly, Category = "Phase")
	bool bIsPhaseActive;

	// ������ ����
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SpawnedEnemies;

	// GameMode ����
	UPROPERTY()
	TObjectPtr<ADRStageGameMode> GameMode;

	// GameState ����
	UPROPERTY()
	TObjectPtr<ADRStageGameState> GameState;

	// ������ ��ġ�� ��ü Ŭ���� ����Ʈ 3�� (��� ����� ����)
	UPROPERTY(BlueprintReadOnly, Category = "Phase")
	TArray<TObjectPtr<ADRCleanserSite>> CleanserSites;

	// Phase1���� ���õ� Ȱ�� Ŭ���� ����Ʈ 2��
	// Phase2, 3������ �� ����Ʈ���� ���
	UPROPERTY(BlueprintReadOnly, Category = "Phase")
	TArray<TObjectPtr<ADRCleanserSite>> ActiveCleanserSites;

	// ========== ������ ��ǥ UI ==========
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Config")
	UDataTable* PhaseObjectiveDataTable;
	
	void SetupPhaseObjective(int32 PhaseNumber);
};
