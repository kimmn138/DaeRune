// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRMainMenuGameMode.generated.h"

class ADRCharacter;
class UDRTutorialStartWidget;

UCLASS()
class DAERUNE_API ADRMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADRMainMenuGameMode();

	// 튜토리얼 시작 (UI에서 호출)
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void StartTutorial();

protected:
	virtual void BeginPlay() override;

	// 튜토리얼 완료 여부 확인
	bool HasCompletedTutorial() const;

	// ========== 서브레벨 관리 ==========

	// 서브레벨 로드 완료 콜백
	UFUNCTION()
	void OnTutorialPreviewLevelLoaded();
	UFUNCTION()
	void OnLobbyPreviewLevelLoaded();

	// 태그로 CameraActor를 찾아 ViewTarget 설정
	void SetViewTargetByTag(APlayerController* PC, const FName& Tag);

	// ========== 튜토리얼 프리뷰 ==========

	// 디스플레이 캐릭터 스폰 (튜토리얼 프리뷰용, 비점유)
	void SpawnTutorialDisplayCharacter();

	// 튜토리얼 시작 UI 표시
	void ShowTutorialStartUI(APlayerController* PC);

	// 로비 메뉴 UI 표시 (기존 UMenu)
	void ShowLobbyMenuUI(APlayerController* PC);

	// 튜토리얼 전환 완료 후 맵 이동
	void OnTutorialTransitionFinished();

	// ========== 에디터 설정 프로퍼티 ==========

	// 튜토리얼 시작 위젯 클래스 (WBP_TutorialStartMenu)
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UDRTutorialStartWidget> TutorialStartWidgetClass;

	// 로비 메뉴 위젯 클래스 (기존 UMenu BP)
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> LobbyMenuWidgetClass;

	// 디스플레이 캐릭터 블루프린트 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Character")
	TSubclassOf<ADRCharacter> TutorialCharacterClass;

	// 서브레벨 이름
	UPROPERTY(EditDefaultsOnly, Category = "Level")
	FName TutorialPreviewLevelName = TEXT("SL_TutorialPreview");

	UPROPERTY(EditDefaultsOnly, Category = "Level")
	FName LobbyPreviewLevelName = TEXT("SL_LobbyPreview");

	// 카메라 태그
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	FName TutorialCameraTag = TEXT("TutorialPreviewCamera");

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	FName LobbyCameraTag = TEXT("LobbyPreviewCamera");

	// 캐릭터 스폰 위치 태그
	UPROPERTY(EditDefaultsOnly, Category = "Character")
	FName TutorialCharacterSpawnTag = TEXT("TutorialCharacterSpawnPoint");

	// 카메라 블렌드 시간
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraBlendTime = 2.0f;

	// 튜토리얼 맵 이름
	UPROPERTY(EditDefaultsOnly, Category = "Level")
	FString TutorialMapName = TEXT("TutorialMap");

private:
	// 현재 활성 UI 위젯
	UPROPERTY()
	TObjectPtr<UUserWidget> CurrentMenuWidget;

	// 디스플레이 캐릭터
	UPROPERTY()
	TObjectPtr<ADRCharacter> DisplayCharacter;

	// 현재 모드
	bool bIsTutorialMode = false;

	FTimerHandle TutorialTransitionTimerHandle;
};
