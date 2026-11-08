// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRMainMenuGameMode.generated.h"

class UDRTutorialStartWidget;
class UDRLoadingScreenWidget;
class UDRLoadingScreenSubsystem;

UCLASS()
class DAERUNE_API ADRMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADRMainMenuGameMode();

	// 튜토리얼 시작 (UI에서 호출)
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void StartTutorial();

	// 튜토리얼 스킵: 완료 플래그를 저장하고 메인 메뉴를 재로드 → 로비 프리뷰로 전환
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void SkipTutorialAndReloadMenu();

	// QA: 튜토리얼 진행 상태를 리셋하고 메인 메뉴를 재로드
	UFUNCTION(BlueprintCallable, Category = "MainMenu|QA")
	void ResetTutorialAndReloadMenu();

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

	// ========== 첫 화면 공개 ==========
	// 배경·캐릭터가 전부 서브레벨에 있고, 패키지 빌드에서는 메시가 PSO 컴파일을 마쳐야 그려진다
	// (r.PSOPrecache.ProxyCreationWhenPSOReady) — 그 전에 보이면 배경 → 캐릭터 순으로 튀어나오므로
	// BeginPlay 부터 검게 덮어 두고, 서브레벨 메시가 모두 준비되면 페이드 인 + 메뉴 UI 를 띄운다

	// 서브레벨이 보이게 된 뒤 준비 확인을 시작한다
	void BeginRevealWhenReady(FName LevelName);

	// 준비 확인(반복 타이머) — 준비됐거나 MaxRevealWaitTime 이 지나면 RevealHoldTime 뒤 RevealMenu
	void CheckRevealReady();

	// 서브레벨의 보이는 프리미티브 중 PSO 를 아직 컴파일하는 것이 없으면 true
	bool IsRevealLevelReady() const;

	// 페이드 인 + 메뉴 UI 표시
	void RevealMenu();

	// 첫 화면이 검은 화면에서 밝아지는 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "Reveal", meta = (ClampMin = "0"))
	float RevealFadeInTime = 0.5f;

	// 메시가 모두 준비된 뒤 페이드 인 전에 더 기다리는 시간(초) — 다시 만들어진 프록시의 첫 그리기와 조명(GI)이 자리 잡을 시간
	UPROPERTY(EditDefaultsOnly, Category = "Reveal", meta = (ClampMin = "0"))
	float RevealHoldTime = 0.3f;

	// 메시가 준비되지 않아도 이 시간(초)이 지나면 화면을 연다(PSO 컴파일이 끝나지 않는 경우의 안전장치)
	UPROPERTY(EditDefaultsOnly, Category = "Reveal", meta = (ClampMin = "0"))
	float MaxRevealWaitTime = 5.f;

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

	// 로비(대기실) 맵 경로 · 최대 인원 — 방 만들기 · 참가 메뉴에 넘긴다
	UPROPERTY(EditDefaultsOnly, Category = "Level")
	FString LobbyMapPath = TEXT("/Game/Maps/LobbyMap");

	UPROPERTY(EditDefaultsOnly, Category = "Level", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxPublicConnections = 4;

	// 튜토리얼 맵 이름
	UPROPERTY(EditDefaultsOnly, Category = "Level")
	FString TutorialMapName = TEXT("TutorialMap");

	// 튜토리얼 시작 시 사용할 로딩 화면 위젯 클래스(WBP_LoadingScreen_Tutorial 등)
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UDRLoadingScreenWidget> TutorialLoadingWidgetClass;

	// 방 만들기 / 방 참가 로딩 화면 위젯 클래스(WBP_CreateRoomLoading / WBP_InRoomLoading)
	// 메뉴가 입력 검증을 통과해 요청을 보낼 때 띄우고, 요청이 실패하면 닫는다
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UDRLoadingScreenWidget> CreateRoomLoadingWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UDRLoadingScreenWidget> JoinRoomLoadingWidgetClass;

	// 로비 메뉴(UMenu)의 방 만들기 · 참가 요청 알림
	UFUNCTION()
	void HandleHostRequestStarted();
	UFUNCTION()
	void HandleJoinRequestStarted();
	UFUNCTION()
	void HandleSessionRequestFailed();

private:
	// 현재 활성 UI 위젯
	UPROPERTY()
	TObjectPtr<UUserWidget> CurrentMenuWidget;

	// 현재 모드
	bool bIsTutorialMode = false;

	// 첫 화면 공개 대기 중인 서브레벨 · 대기 시작 시각(실시간) · 확인 타이머
	FName RevealLevelName;
	double RevealWaitStartTime = 0.0;
	FTimerHandle RevealTimerHandle;

	// 메뉴 위젯을 닫고 현재 메인 메뉴 맵을 다시 연다 → BeginPlay 가 튜토리얼/로비 분기를 다시 평가
	void ReloadMenu();

	UDRLoadingScreenSubsystem* GetLoadingScreenSubsystem() const;
};
