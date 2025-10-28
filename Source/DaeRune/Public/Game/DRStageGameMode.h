// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameModeBase.h"
#include "DRStageGameMode.generated.h"

class UDRPhaseBase;
class ADRStageGameState;
class ADRCleanserSite;

/**
 * 스테이지 게임 모드
 * 4개 페이즈 시스템을 관리하는 서버 권한 클래스
 */
UCLASS()
class DAERUNE_API ADRStageGameMode : public ADRGameModeBase
{
	GENERATED_BODY()
	
public:
	ADRStageGameMode();

	// 페이즈 완료 조건을 만족했는지 확인
	UFUNCTION(BlueprintCallable, Category = "Phase")
	bool ValidatePhaseCompletion();

protected:
	virtual void BeginPlay() override;
	virtual void HandleWipeout() override;

	// 로비 맵으로 이동
	void ReturnToLobby();

	// 로비 맵 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage|Config")
	FString LobbyMapName = TEXT("LobbyMap");

	// ========== 클렌저 사이트 ==========

	// 클렌저 사이트를 찾을 때 사용할 태그
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage|CleanserSites")
	FName CleanserSiteTag = TEXT("CleanserSite");

	// ========== 페이즈 관리 ==========
	UFUNCTION(BlueprintCallable, Category = "Phase")
	void InitializePhaseSystem();

	UFUNCTION(BlueprintCallable, Category = "Phase")
	void StartPhase(int32 PhaseIndex);

	UFUNCTION(BlueprintCallable, Category = "Phase")
	void EndCurrentPhase();

	// 다음 페이즈로 전환하기 위해 호출
	UFUNCTION(BlueprintCallable, Category = "Phase")
	void TransitionToNextPhase();

private:
	// 현재 실행 중인 페이즈
	UPROPERTY()
	TObjectPtr<UDRPhaseBase> CurrentPhase;

	// 모든 페이즈 클래스들
	UPROPERTY(EditDefaultsOnly, Category = "Phase")
	TArray<TSubclassOf<UDRPhaseBase>> PhaseClasses;

	// 생성된 페이즈 인스턴스들
	UPROPERTY()
	TArray<TObjectPtr<UDRPhaseBase>> PhaseInstances;

	// GameState 캐싱
	TObjectPtr<ADRStageGameState> CachedGameState;

	// 레벨에서 찾은 클렌저 사이트들 (BeginPlay에서 자동 탐색)
	UPROPERTY()
	TArray<TObjectPtr<ADRCleanserSite>> CleanserSites;
};
