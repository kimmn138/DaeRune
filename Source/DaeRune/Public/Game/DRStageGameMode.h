// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameModeBase.h"
#include "DRStageGameMode.generated.h"

class UDRPhaseBase;
class ADRStageGameState;
class ADRCleanserSite;

/**
 * �������� ���� ���
 * 4�� ������ �ý����� �����ϴ� ���� ���� Ŭ����
 */
UCLASS()
class DAERUNE_API ADRStageGameMode : public ADRGameModeBase
{
	GENERATED_BODY()
	
public:
	ADRStageGameMode();

	// 플레이어별 DefaultPawnClass 결정
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	// ������ �Ϸ� ������ �����ߴ��� Ȯ��
	UFUNCTION(BlueprintCallable, Category = "Phase")
	bool ValidatePhaseCompletion();

	UDRPhaseBase* GetCurrentPhase() { return CurrentPhase; }

	// Phase�� ���� ���� Ʈ����
	UFUNCTION()
	void TriggerGameOver();

	UFUNCTION()
	void TriggerGameClear();

	// ���� ������� ��ȯ�ϱ� ���� ȣ��
	UFUNCTION(BlueprintCallable, Category = "Phase")
	void TransitionToNextPhase();

protected:
	virtual void BeginPlay() override;
	virtual void HandleWipeout() override;

	void BlockJoinInProgress();

	// �κ� ������ �̵�
	void ReturnToLobby();


	// 모든 플레이어에게 게임 종료 알림 (단일 순회로 최적화)
	void NotifyAllPlayersGameEnd(bool bIsGameClear);

	// �κ� �� �̸�
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage|Config")
	FString LobbyMapName = TEXT("LobbyMap");

	// ========== Ŭ���� ����Ʈ ==========

	// Ŭ���� ����Ʈ�� ã�� �� ����� �±�
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage|CleanserSites")
	FName CleanserSiteTag = TEXT("CleanserSite");

	// ========== ������ ���� ==========
	UFUNCTION(BlueprintCallable, Category = "Phase")
	void InitializePhaseSystem();

	UFUNCTION(BlueprintCallable, Category = "Phase")
	void StartPhase(int32 PhaseIndex);

	UFUNCTION(BlueprintCallable, Category = "Phase")
	void EndCurrentPhase();

private:
	// 현재 실행 중인 페이즈
	UPROPERTY()
	TObjectPtr<UDRPhaseBase> CurrentPhase;

	// 모든 페이즈 클래스들
	UPROPERTY(EditDefaultsOnly, Category = "Phase")
	TArray<TSubclassOf<UDRPhaseBase>> PhaseClasses;

	// 페이즈별 인스턴스들
	UPROPERTY()
	TArray<TObjectPtr<UDRPhaseBase>> PhaseInstances;

	// GameState 캐시
	TObjectPtr<ADRStageGameState> CachedGameState;

	// 레벨에서 찾은 클렌저 사이트들 (BeginPlay에서 자동 탐색)
	UPROPERTY()
	TArray<TObjectPtr<ADRCleanserSite>> CleanserSites;

	// Phase 전환 중 Race Condition 방지 플래그
	bool bIsTransitioningPhase = false;
};
