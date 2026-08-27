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

	// 페이즈 진입 배너 문구 (Plan6 §5.3)
	// 비어 있으면 배너를 띄우지 않는다 - 한 페이즈 안에서 목표만 교체하는 서브 목표 전환용.
	// 비어 있는 경우 OverlayWidgetController가 레거시 switch 문구로 폴백한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText PhaseAlarmText;
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

	// ========== 플레이어 상태 통지 (Plan6 §14.3.5) ==========
	// GameMode가 사망/접속 종료를 현재 페이즈에 전달한다.
	// 스테이지2 방3+4처럼 특정 플레이어의 이탈이 진행 상태를 되돌려야 하는 페이즈가 사용한다.

	// 플레이어 사망 (아직 게임에는 남아 있음)
	virtual void NotifyPlayerDied(APlayerState* DeadPlayerState) {}

	// 플레이어 접속 종료 (영구 이탈)
	virtual void NotifyPlayerLeft(APlayerState* LeftPlayerState) {}

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

	// 행 이름을 직접 지정해 목표를 설정한다 (Plan6 §5.2).
	// 한 페이즈 안에서 목표를 여러 번 교체하는 스테이지2 페이즈들이 사용한다.
	void SetupPhaseObjectiveByRow(FName RowName);

	// 위와 동일하되 RequiredCount(진행도 분모)를 런타임 값으로 덮어쓴다.
	// 인원별 스폰 수, 두더지 목표 수처럼 실행 중에 분모가 정해지는 목표에 사용한다.
	void SetupPhaseObjectiveByRow(FName RowName, int32 OverrideRequiredCount);
};
