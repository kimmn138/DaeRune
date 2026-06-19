// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DRLoadingScreenSubsystem.generated.h"

class UDRLoadingScreenWidget;
class SWidget;

/**
 * 맵 전환 로딩 화면 오케스트레이션.
 *
 * GameInstanceSubsystem이라 맵 전환에도 파괴되지 않으므로, "등장 애니 → 로드 → 퇴장 애니"
 * 시퀀스를 맵 경계 너머로 이어줄 수 있다.
 *
 * 흐름:
 *  1) 현재 맵에서 위젯 생성 + PlayIntro (게임 스레드 살아있음 → 슬라이드 인 정상)
 *  2) 실제 맵 로드 (이 찰나의 멈춤 구간은 MoviePlayer 브리지로 동일 모습 + Throbber 표시)
 *  3) PostLoadMapWithWorld(새 맵)에서 위젯을 새로 생성 + PlayOutro → 종료 시 제거
 *
 *  - 인트로/아웃트로 위젯은 같은 클래스의 서로 다른 인스턴스다(맵 경계로 UObject가
 *    무효화되는 문제를 피하기 위해 보존하지 않고 새로 만든다). 위젯의 기본 레이아웃이
 *    '표시 완료' 상태이므로 시각적으로 연속되어 보인다.
 */
UCLASS()
class DAERUNE_API UDRLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 등장 애니를 재생하고, 다음 맵 로드 완료 시 퇴장 애니를 재생하도록 예약한다.
	 * 트래블을 외부(세션 플러그인 등)에서 시작하는 방 생성/방 참가에 사용.
	 * (이 경우 인트로 재생과 네트워크/트래블이 병행되며, 멈춤 구간은 MoviePlayer가 덮는다.)
	 */
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void BeginLoadingScreen(TSubclassOf<UDRLoadingScreenWidget> WidgetClass);

	/**
	 * 등장 애니가 끝난 뒤 OpenLevel을 호출하는 로컬 하드 로드용(튜토리얼 등).
	 * 진짜 "애니 후 로드"가 필요할 때 사용.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void OpenLevelWithLoadingScreen(TSubclassOf<UDRLoadingScreenWidget> WidgetClass, FName LevelName, const FString& Options);

	/**
	 * 심리스 트래블(로비→스테이지 등)에서 사용할 로딩 위젯 클래스를 지정한다.
	 * 심리스 트래블 시작은 서버/각 클라이언트에서 로컬로 발생하므로, 모든 클라이언트가
	 * 로딩 화면을 보려면 각 클라이언트가 로비에 있을 때 이 함수를 호출해 클래스를 등록해야 한다.
	 * (예: 로비 HUD/위젯의 BeginPlay 또는 Construct에서 호출 — 클라마다 실행됨)
	 */
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void SetSeamlessLoadingWidgetClass(TSubclassOf<UDRLoadingScreenWidget> WidgetClass);

	/**
	 * 심리스 트래블로 떠 있던 로딩 화면을 닫는다(퇴장 애니 후 제거).
	 * 도착한 맵(스테이지)에서 준비가 끝난 시점에 호출 — 예: 스테이지 HUD/GameState BeginPlay.
	 * 심리스 트래블은 PostLoadMapWithWorld가 호출되지 않으므로 도착 측에서 명시적으로 불러야 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void FinishLoadingScreen();

	// 멈춤(블로킹 로드) 구간에서 MoviePlayer 화면이 노출될 최소 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading")
	float MinimumFreezeScreenTime = 0.5f;

	// WBP가 NotifyIntroFinished를 호출하지 않을 때 강제로 로드를 진행하는 최대 대기 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading")
	float MaxIntroWaitTime = 4.0f;

	// 심리스 로딩 화면이 최소한 표시될 시간(초). 로드가 더 빨라도 이 시간 동안은 유지(깜빡임 방지).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading")
	float MinimumSeamlessDisplayTime = 1.5f;

private:
	void HandlePostLoadMap(UWorld* LoadedWorld);

	// 심리스 트래블 시작 시(각 인스턴스 로컬) 로딩 위젯을 띄운다.
	void HandleSeamlessTravelStart(UWorld* CurrentWorld, const FString& LevelName);

	// 심리스 로딩 화면의 실제 퇴장(아웃트로 + 제거). FinishLoadingScreen이 최소 표시 시간 후 호출.
	void PlaySeamlessOutro();

	// 위젯 생성 + 뷰포트 추가. (서버/위젯클래스 없음이면 nullptr)
	// bPersistent=true면 AddViewportWidgetContent로 뷰포트에 직접 붙여 월드 전환(심리스 트래블)에도 유지.
	UDRLoadingScreenWidget* CreateAndShow(TSubclassOf<UDRLoadingScreenWidget> WidgetClass, bool bPersistent = false);

	// AddViewportWidgetContent로 붙인 유지 위젯을 뷰포트에서 제거(저장해 둔 Slate 인스턴스 사용).
	void RemovePersistentWidget();

	// 멈춤 구간용 MoviePlayer 로딩 화면 등록(같은 클래스의 별도 인스턴스).
	void SetupMoviePlayerBridge(TSubclassOf<UDRLoadingScreenWidget> WidgetClass);

	// 현재 화면에 떠 있는 위젯(인트로 또는 아웃트로).
	UPROPERTY()
	TObjectPtr<UDRLoadingScreenWidget> ActiveWidget;

	// 다음 맵 로드 완료 시 띄울 아웃트로 위젯 클래스(없으면 아웃트로 생략).
	UPROPERTY()
	TSubclassOf<UDRLoadingScreenWidget> PendingOutroClass;

	// 심리스 트래블 시 사용할 로딩 위젯 클래스(각 클라이언트가 로비에서 등록).
	UPROPERTY()
	TSubclassOf<UDRLoadingScreenWidget> SeamlessTravelWidgetClass;

	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle SeamlessTravelStartHandle;

	// 유지(persistent) 위젯으로 뷰포트에 직접 붙인 Slate 인스턴스(제거 시 이 인스턴스를 사용).
	TSharedPtr<SWidget> PersistentSlateWidget;

	// 심리스 로딩 화면이 표시되기 시작한 실제 시각(월드 독립적, 최소 표시 시간 계산용).
	double SeamlessShowStartTime = 0.0;

	// 최소 표시 시간을 채운 뒤 퇴장을 실행하기 위한 타이머.
	FTimerHandle SeamlessOutroTimerHandle;
};
