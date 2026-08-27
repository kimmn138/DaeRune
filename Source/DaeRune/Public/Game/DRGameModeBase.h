// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "DRGameModeBase.generated.h"

class UAbilityInfo;
class UCharacterClassInfo;
class UGameBalanceConfig;

/**
 * DaeRune �⺻ ���� ��� Ŭ����
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

	// �����Ƽ ���� ������ ����
	UPROPERTY(EditDefaultsOnly, Category = "Ability Info")
	TObjectPtr<UAbilityInfo> AbilityInfo;

	// 게임 밸런스 설정 DataAsset
	UPROPERTY(EditDefaultsOnly, Category = "Game Balance")
	TObjectPtr<UGameBalanceConfig> GameBalanceConfig;

	// �÷��̾ ������� �� ȣ��
	void OnPlayerDied(APlayerState* DeadPlayer);

	// �� ���� üũ
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

	// ���� �� ó�� ���۱��� ��� �ð�
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameMode|Config")
	float WipeoutDelayTime = 3.0f;

	// ���� ó�� Ÿ�̸� �ڵ�
	FTimerHandle WipeoutTimerHandle;

	// ���� ó�� ������ ����
	bool bIsWipeoutInProgress = false;
};
