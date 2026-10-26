// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "DRGameModeBase.generated.h"

class UAbilityInfo;
class UCharacterClassInfo;
class UGameBalanceConfig;
class ADRCharacter;
enum class EPlayerCharacterClass : uint8;

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

	/**
	 * 컨트롤러에 PawnClass 의 새 폰을 만들어 빙의시킨다 (서버). 기존 폰은 파괴한다.
	 * ASC 는 PlayerState 에 있어 어빌리티 · 효과 · 속성이 그대로 이어진다 — 클래스를 바꾸는 쪽(로비)은 미리 비운다.
	 */
	APawn* RespawnPlayerPawn(AController* Controller, TSubclassOf<APawn> PawnClass, const FTransform& SpawnTransform);

	/**
	 * 사망한 플레이어를 At 에서 되살린다 (서버).
	 * 사망 직후라 폰이 남아 있으면 그 폰을 되살리고, 시체가 이미 정리됐으면 같은 클래스의 새 폰을 만든다.
	 * 살아 있는 플레이어면 아무것도 하지 않고 false.
	 */
	bool RevivePlayer(APlayerController* PC, const FTransform& At, float HealthRatio, float WaterRatio);

	// 플레이어 캐릭터 클래스 → 캐릭터 BP (클래스 정보에 없으면 null)
	TSubclassOf<ADRCharacter> FindPlayerCharacterClass(EPlayerCharacterClass CharacterClass) const;

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
