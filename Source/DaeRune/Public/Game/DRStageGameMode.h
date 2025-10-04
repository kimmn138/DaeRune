// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameModeBase.h"
#include "DRStageGameMode.generated.h"

class ADRStageGameState;

/**
 * 스테이지 페이즈 열거형
 * 4개의 페이즈가 순차적으로 진행됨
 */
UENUM(BlueprintType)
enum class EStagePhase : uint8
{
	None		UMETA(DisplayName = "None"),
	Secure		UMETA(DisplayName = "Secure"),			// 1단계: 확보
	Combine		UMETA(DisplayName = "Combine"),			// 2단계: 결합
	Defend		UMETA(DisplayName = "Defend"),			// 3단계: 방어
	Eliminate	UMETA(DisplayName = "Eliminate"),		// 4단계: 처치
	Completed	UMETA(DisplayName = "Completed")		// 스테이지 클리어
};

/**
 * 페이즈별 설정 구조체
 * 나중에 DataTable로 외부화 가능
 */
USTRUCT(BlueprintType)
struct FStagePhaseConfig
{
	GENERATED_BODY()

	// 페이즈 제한 시간 (0이면 무제한)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeLimit = 0.f;

	// 페이즈 시작 시 표시할 메시지
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText PhaseStartMessage;

	FStagePhaseConfig()
		: TimeLimit(0.f)
	{
	}
};

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

protected:
	virtual void HandleWipeout() override;

	// 로비 맵으로 이동
	void ReturnToLobby();

	// 로비 맵 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage|Config")
	FString LobbyMapName = TEXT("StartupMap");
};
