// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Phase/DRPhaseBase.h"
#include "DRPhase2.generated.h"

class ADRCleanserSite;
class ADRCleanserPart;
class ADREnemy;

/**
 * Phase 2: ��ǰ ���� �� ��ġ
 * - ���� 4������ ��ǰ�� ��� ����ġ�� �� ����
 * - �÷��̾ ��ǰ�� �����Ͽ� Ŭ���� ����Ʈ�� ��ġ
 * - �� Ŭ���� ����Ʈ�� 2���� �� 4�� ��ġ �� �Ϸ�
 */
// [DEPRECATED] 페이즈 구조 개편(2026-06)으로 본 클래스의 책임은 UDRPhase1에 통합되었습니다.
// BP_DRStageGameMode::PhaseClasses 배열에서 본 클래스 참조를 제거한 뒤, 다음 정리 PR에서
// 본 .h/.cpp 파일을 삭제하세요. 클래스 이름을 변경하면 BP 참조가 깨지므로 이름은 유지합니다.
UCLASS(Blueprintable)
class DAERUNE_API UDRPhase2 : public UDRPhaseBase
{
	GENERATED_BODY()
	
public:
	virtual void OnPhaseStart() override;
	virtual void OnPhaseEnd() override;

protected:
	// ========== ���� �Լ� ==========

	// ���� ����Ʈ ã��
	UFUNCTION(BlueprintCallable, Category = "Phase2")
	void FindEnemySpawnPoints();

	// ��ǰ�� ��� ����ġ�� �� ����
	UFUNCTION(BlueprintCallable, Category = "Phase2")
	void SpawnPartCarryingEnemies();

	// Ư�� ��ġ�� �� ����
	UFUNCTION(BlueprintCallable, Category = "Phase2")
	ADREnemy* SpawnEnemyAtLocation(AActor* SpawnPoint);

	// ========== �̺�Ʈ ó�� ==========

	// ��ǰ ��ġ �Ϸ� �̺�Ʈ
	UFUNCTION()
	void OnPartInstalled(ADRCleanserSite* Site);

	// ������ �Ϸ� ���� üũ
	UFUNCTION(BlueprintCallable, Category = "Phase2")
	void CheckPhaseCompletion();

	// ========== ���� ���� ==========

	// ��ǰ�� ��� ����ġ�� �� Ŭ����
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase2|Config")
	TSubclassOf<ADREnemy> PartCarryingEnemyClass;

	// ���� ����Ʈ�� ã�� �±� �̸�
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase2|Config")
	FName SpawnPointTag = "Phase2SpawnPoint";

	// �� ���� ��ġ ���͵� (4��)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Phase2|Config")
	TArray<TObjectPtr<AActor>> EnemySpawnPoints;

private:
	// ��ġ �Ϸ�� Ŭ���� ����Ʈ ����
	UPROPERTY()
	TSet<TObjectPtr<ADRCleanserSite>> CompletedSites;
};
