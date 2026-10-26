// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "DRGameModeBase.generated.h"

class UAbilityInfo;
class UCharacterClassInfo;
class UGameBalanceConfig;

/**
 * DaeRune 기본 게임 모드 클래스
 */
UCLASS()
class DAERUNE_API ADRGameModeBase : public AGameMode
{
	GENERATED_BODY()
	
public:
	ADRGameModeBase();

	// 적 전용 CharacterClassInfo
	UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
	TObjectPtr<UCharacterClassInfo> EnemyCharacterClassInfo;

	// 어빌리티 정보 데이터 에셋
	UPROPERTY(EditDefaultsOnly, Category = "Ability Info")
	TObjectPtr<UAbilityInfo> AbilityInfo;

	// 게임 밸런스 설정 DataAsset
	UPROPERTY(EditDefaultsOnly, Category = "Game Balance")
	TObjectPtr<UGameBalanceConfig> GameBalanceConfig;

	// 플레이어가 사망했을 때 호출
	void OnPlayerDied(APlayerState* DeadPlayer);

	// 팀 전멸 체크
	virtual bool CheckTeamWipeout();

	// 플레이어 접속 종료 (Plan6 §5.10)
	// 프로젝트에 Logout 오버라이드가 없어 신규 추가한다.
	virtual void Logout(AController* Exiting) override;

protected:
	virtual void BeginPlay() override;

	// 사망/이탈을 현재 페이즈에 통지 (스테이지 게임모드가 오버라이드)
	virtual void NotifyPhasePlayerDied(APlayerState* DeadPlayerState) {}
	virtual void NotifyPhasePlayerLeft(APlayerState* LeftPlayerState) {}

	// 전멸 시 처리
	virtual void HandleWipeout();

	// 맵 전환 전 정리 작업 (설정창 닫기, 오디오 정리)
	virtual void PrepareForTravel();

	// 전멸 후 처리 시작까지 대기 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameMode|Config")
	float WipeoutDelayTime = 3.0f;

	// 전멸 처리 타이머 핸들
	FTimerHandle WipeoutTimerHandle;

	// 전멸 처리 중인지 여부
	bool bIsWipeoutInProgress = false;
};
