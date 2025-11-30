// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "DRGameModeBase.generated.h"

class UAbilityInfo;
class UCharacterClassInfo;
class ADRDetectionManager;

/**
 * DaeRune 기본 게임 모드 클래스
 */
UCLASS()
class DAERUNE_API ADRGameModeBase : public AGameMode
{
	GENERATED_BODY()
	
public:
	ADRGameModeBase();

	// 캐릭터 클래스별 기본 설정
	UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
	TObjectPtr<UCharacterClassInfo> CharacterClassInfo;

	// 어빌리티 정보 데이터 에셋
	UPROPERTY(EditDefaultsOnly, Category = "Ability Info")
	TObjectPtr<UAbilityInfo> AbilityInfo;

	// 플레이어가 사망했을 때 호출
	void OnPlayerDied(APlayerState* DeadPlayer);

	// 팀 전멸 체크
	virtual bool CheckTeamWipeout();
	
protected:
	virtual void BeginPlay() override;
	// 전멸 시 처리
	virtual void HandleWipeout();

	// 전멸 후 처리 시작까지 대기 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameMode|Config")
	float WipeoutDelayTime = 3.0f;

	// 전멸 처리 타이머 핸들
	FTimerHandle WipeoutTimerHandle;

	// 전멸 처리 중인지 여부
	bool bIsWipeoutInProgress = false;

	// 탐지 매니저 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Detection")
	TSubclassOf<ADRDetectionManager> DetectionManagerClass;

	// 스폰된 탐지 매니저 인스턴스
	UPROPERTY()
	TObjectPtr<ADRDetectionManager> DetectionManager;

	// 탐지 매니저 스폰
	void SpawnDetectionManager();
};
