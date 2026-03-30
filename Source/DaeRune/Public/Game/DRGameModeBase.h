// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "DRGameModeBase.generated.h"

class UAbilityInfo;
class UCharacterClassInfo;
class UGameBalanceConfig;
class ADRDetectionManager;

/**
 * DaeRune �⺻ ���� ��� Ŭ����
 */
UCLASS()
class DAERUNE_API ADRGameModeBase : public AGameMode
{
	GENERATED_BODY()
	
public:
	ADRGameModeBase();

	// ĳ���� Ŭ������ �⺻ ����
	UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
	TObjectPtr<UCharacterClassInfo> CharacterClassInfo;

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
	
protected:
	virtual void BeginPlay() override;

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

	// Ž�� �Ŵ��� Ŭ����
	UPROPERTY(EditDefaultsOnly, Category = "Detection")
	TSubclassOf<ADRDetectionManager> DetectionManagerClass;

	// ������ Ž�� �Ŵ��� �ν��Ͻ�
	UPROPERTY()
	TObjectPtr<ADRDetectionManager> DetectionManager;

	// Ž�� �Ŵ��� ����
	void SpawnDetectionManager();
};
