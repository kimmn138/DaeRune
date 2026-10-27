// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameModeBase.h"
#include "DRStageGameMode.generated.h"

class UDRPhaseBase;
class ADRStageGameState;
class ADRCleanserSite;
class ADRPlayerController;

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

	// 플레이어별 DefaultPawnClass 결정
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	// 페이즈 완료 조건을 만족했는지 확인
	UFUNCTION(BlueprintCallable, Category = "Phase")
	bool ValidatePhaseCompletion();

	UDRPhaseBase* GetCurrentPhase() { return CurrentPhase; }

protected:
	// 사망/이탈을 현재 페이즈에 전달 (Plan6 §14.3.5)
	virtual void NotifyPhasePlayerDied(APlayerState* DeadPlayerState) override;
	virtual void NotifyPhasePlayerLeft(APlayerState* LeftPlayerState) override;

public:

	// Phase별 게임 오버 트리거
	UFUNCTION()
	void TriggerGameOver();

	UFUNCTION()
	void TriggerGameClear();

	// 다음 페이즈로 전환하기 위해 호출
	UFUNCTION(BlueprintCallable, Category = "Phase")
	void TransitionToNextPhase();

protected:
	virtual void BeginPlay() override;
	virtual void HandleWipeout() override;

	// 로비 맵으로 이동
	void ReturnToLobby();


	// 모든 플레이어에게 게임 종료 알림 (단일 순회로 최적화)
	// 보상 리포트를 결과 UI 표시보다 먼저 보낸다.
	void NotifyAllPlayersGameEnd(bool bIsGameClear);

	// ========== 재화 보상 (Plan2.md 8.7) ==========

public:
	// 이 스테이지의 식별자. ProgressionConfig 의 StageRewards / 업적 정의와 일치해야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage|Config")
	FName StageId = TEXT("Stage1");

	// 업적/클리어 조건 달성 보고 (서버 전용). PC == nullptr 이면 전원에게 부여한다.
	// 페이즈/BP 는 판정 로직을 각자 두지 말고 이 함수만 호출한다.
	UFUNCTION(BlueprintCallable, Category = "Stage|Achievement")
	void GrantStageAchievement(FName AchievementId, ADRPlayerController* PC = nullptr);

	// 총 페이즈 수 (PhaseClasses 는 private 이라 외부 조회용)
	UFUNCTION(BlueprintPure, Category = "Phase")
	int32 GetTotalPhaseCount() const { return PhaseClasses.Num(); }

protected:

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

private:
	// 스테이지 종료 (클리어 · 게임 오버 공통): 페이즈 정리 → 결과 사운드 · UI → 잠시 뒤 로비로
	void EndStage(bool bCleared);

	// 현재 페이즈가 진행 중일 때만 OnPhaseEnd 를 부른다.
	// EndCurrentPhase → StartPhase, 게임 오버/클리어가 겹쳐도 파생 페이즈의 정리 코드가 두 번 돌지 않게 한다.
	void EndCurrentPhaseIfActive();

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
	UPROPERTY()
	TObjectPtr<ADRStageGameState> CachedGameState;

	// 레벨에서 찾은 클렌저 사이트들 (BeginPlay에서 자동 탐색)
	UPROPERTY()
	TArray<TObjectPtr<ADRCleanserSite>> CleanserSites;

	// Phase 전환 중 Race Condition 방지 플래그
	bool bIsTransitioningPhase = false;

	// 플레이어별 달성 업적 (스테이지 종료 시 일괄 전송)
	TMap<TWeakObjectPtr<ADRPlayerController>, TArray<FName>> PendingAchievements;

	// 전원 대상 업적 (GrantStageAchievement 의 PC == nullptr 경로)
	TArray<FName> GlobalPendingAchievements;
};
