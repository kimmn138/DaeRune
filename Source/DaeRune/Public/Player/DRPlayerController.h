// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "GenericTeamAgentInterface.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRProgressionTypes.h"
#include "DRPlayerController.generated.h"

// ��ȣ�ۿ� ��������Ʈ
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractPressed);

class UDamageTextComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UDRInputConfig;
class UDRAbilitySystemComponent;
class ADRCleanserPart;
class ADRCleanserSite;
class ADRRobotVacuumCharacter;
class ADRWaitingRoomCameraActor;
class UDRWaitingRoomWidget;
class ADRS2InteractProp;
class ADRS2SlidePuzzle;

// 스테이지2 8퍼즐 UI 열기 요청 (Plan6 §14.2.2 - UI 방식).
// HUD/BP 가 이 델리게이트를 받아 실제 위젯을 생성한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlidePuzzleUIRequested, ADRS2SlidePuzzle*, Puzzle);
enum class ELobbyState : uint8;

// 현재 레벨 컨텍스트 (레벨 진입 시 1회 판별해 캐시 - 매 프레임 맵 이름 문자열 연산 방지)
enum class EDRLevelContext : uint8
{
	Unknown,
	MainMenu,
	Lobby,
	Tutorial,
	GameLevel
};

/**
 * DaeRune �÷��̾��� �Է� ó�� �� UI ���� Ŭ����
 */
UCLASS()
class DAERUNE_API ADRPlayerController : public APlayerController, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	ADRPlayerController();

	// Team Interface
	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override { TeamId = NewTeamId; }
	
	// ������ ��ġ ǥ��
	// (코스메틱이라 한두 개 유실돼도 무방 - Unreliable로 reliable 큐 부담 제거)
	UFUNCTION(Client, Unreliable)
	void ShowDamageNumber(float DamageAmount, ACharacter* TargetCharacter);

	// ���� ���� ���� ó��
	UFUNCTION(BlueprintCallable, Category = "Corruption")
	void CorruptedStateChanged(bool bIsStateChanged);

	// �Ʊ�/�� ���� ǥ�� ����
	UFUNCTION(BlueprintImplementableEvent, Category = "Corruption")
	void SetTeamVisualsEnabled(bool bEnabled);

	// ���� ���� Ȯ��
	UFUNCTION(BlueprintCallable, Category = "Corruption")
	bool IsInCorruptedState() const { return bIsCorrupted; }

	// ��ȣ�ۿ� �̺�Ʈ
	UPROPERTY(BlueprintAssignable, Category = "Input")
	FOnInteractPressed OnInteractPressed;

	// ========== ��ǰ �ý��� ==========

	// ��ǰ ���� Ȱ��ȭ/��Ȱ��ȭ
	UFUNCTION(BlueprintCallable, Category = "Part System")
	void SetPartDetectionEnabled(bool bEnabled, class ADRCleanserPart* Part);

	// ����Ʈ���̽����� ��ǰ ã��
	UFUNCTION(BlueprintCallable, Category = "Part System")
	ADRCleanserPart* FindPartByLineTrace();

	// 현재 감지(라인트레이스) 중인 사이트 (IDRInteractable 구현 액터)
	UPROPERTY()
	TObjectPtr<AActor> CurrentOverlappedSite;

	// 사이트 감지 활성화/비활성화 (사이트 박스 오버랩에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Part System")
	void SetSiteDetectionEnabled(bool bEnabled, ADRCleanserSite* Site);

	// 시점 라인트레이스로 사이트 찾기
	UFUNCTION(BlueprintCallable, Category = "Part System")
	ADRCleanserSite* FindSiteByLineTrace();

	// 상호작용 요청 통합 RPC (부품 획득 / 사이트 설치)
	// 서버는 클라이언트가 보낸 포인터를 신뢰하지 않고 거리/상태를 검증한다
	UFUNCTION(Server, Reliable)
	void ServerRequestInteract(AActor* Interactable);

	// 부품 획득 요청의 서버 측 최대 허용 거리
	// (감지 반경 300 + 라인트레이스 250 + 이동/지연 여유)
	UPROPERTY(EditDefaultsOnly, Category = "Part System|Config")
	float MaxInteractDistance = 800.f;

	// ========== 탑승 시스템 (Plan3 Phase B) ==========

	// 청소기 감지 활성화/비활성화 (청소기 MountDetectionSphere 오버랩에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Mount System")
	void SetMountDetectionEnabled(bool bEnabled, ADRRobotVacuumCharacter* Mount);

	// 시점 라인트레이스로 탑승 가능한 청소기 찾기
	UFUNCTION(BlueprintCallable, Category = "Mount System")
	ADRRobotVacuumCharacter* FindMountByLineTrace();

	// 하차 요청 (점프키 → 서버 검증 후 DismountRider)
	UFUNCTION(Server, Reliable)
	void ServerRequestDismount();

	// 탑승 중 좌우 회전 동기화: MOVE_None에선 CMC 무브 패킷이 안 나가 서버가 클라 요를 모르므로 직접 전송
	UFUNCTION(Server, Unreliable)
	void ServerSetMountedYaw(float NewYaw);

	// 지속 돌진 중 S 브레이크 → GA_VacuumDash에 GameplayEvent(Event.Dash.Brake) 전송 (Plan3 §8.2)
	UFUNCTION(Server, Reliable)
	void ServerRequestDashBrake();

	// ��ǰ ȹ�� �� UI ǥ��
	UFUNCTION(BlueprintImplementableEvent, Category = "Part System")
	void OnPartPickedUp();

	UFUNCTION(Client, Reliable)
	void ClientShowPartPickupUI();

	// ========== ���� �ý��� ==========

	// ���� ��� ����
	UPROPERTY(BlueprintReadOnly, Category = "Spectating")
	bool bIsSpectating = false;

	// 대기실 상태 플래그 (ViewTarget 복원 차단용)
	bool bIsInWaitingRoom = false;

	// ���� ���� ���� �÷��̾� �ε���
	int32 CurrentSpectatedPlayerIndex = 0;

	// ���� ���� ���� ĳ���� (���� ����)
	UPROPERTY()
	TWeakObjectPtr<ACharacter> CurrentSpectatedCharacter;

	// ���� ����
	UFUNCTION(Client, Reliable)
	void ClientStartSpectating();

	// ���� ����
	UFUNCTION(Client, Reliable)
	void ClientStopSpectating();

	// ���� �÷��̾�� ��ȯ
	UFUNCTION(BlueprintCallable, Category = "Spectating")
	void SpectateNextPlayer();

	// ���� �÷��̾�� ��ȯ
	UFUNCTION(BlueprintCallable, Category = "Spectating")
	void SpectatePreviousPlayer();

	// ========== ���� �޴� ========== 

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ToggleSettingsMenu();

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void OpenSettingsMenu();

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void CloseSettingsMenu();

	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool IsSettingsMenuOpen() const { return bIsSettingsMenuOpen; }

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleFirstPersonMeshAndHUDVisibility();

	// Blueprint에서 위젯 생성/제거를 구현
	UFUNCTION(BlueprintImplementableEvent, Category = "Settings")
	void OnSettingsMenuOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "Settings")
	void OnSettingsMenuClosed();

	// ========== �Է� ��� ���� ==========

	// ���� ������ �´� �Է� ���� ����
	UFUNCTION(BlueprintCallable, Category = "Input")
	void RestoreDefaultInputMode();

	// ���� ������ ���θ޴����� Ȯ��
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool IsInMainMenu() const;

	// ���� ������ �κ����� Ȯ��
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool IsInLobby() const;

	// ���� �������� Ȯ�� (��������)
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool IsInGameLevel() const;

	// 현재 레벨이 튜토리얼인지 확인
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool IsInTutorial() const;

	// 레벨 진입 시 공통 초기화 (ReceivedPlayer, PostSeamlessTravel에서 호출)
	void OnLevelEntered();

	// ���� ��� UI ǥ��
	UFUNCTION(Client, Reliable)
	void Client_ShowGameOverUI();

	UFUNCTION(Client, Reliable)
	void Client_ShowGameClearUI();

	UFUNCTION(Client, Reliable)
	void Client_ShowTutorialClearUI();

	UFUNCTION(Client, Reliable)
	void ClientStopAllAudio();

	// 레벨 이동 전 설정창 닫기 (서버에서 호출)
	UFUNCTION(Client, Reliable)
	void ClientCloseSettingsMenu();

	// ========== 대기실 카메라 ==========

	// 서버 → 클라이언트: 대기실 슬롯 위치로 텔레포트
	UFUNCTION(Client, Reliable)
	void ClientTeleportToSlot(FVector SlotLocation, FRotator SlotRotation);

	// 서버 → 클라이언트: 대기실 카메라로 ViewTarget 설정
	UFUNCTION(Client, Reliable)
	void ClientSetWaitingRoomView(ADRWaitingRoomCameraActor* CameraActor);

	// 서버 → 클라이언트: 카메라를 캐릭터로 부드럽게 전환
	UFUNCTION(Client, Reliable)
	void ClientStartCameraTransitionToCharacter();

	// 카메라 전환 로직 (Pawn 존재 보장 후 호출)
	void ExecuteCameraTransitionToCharacter();

	// ========== 대기실 카메라 보호 ==========

	/** Possess 시 UE 엔진이 호출하는 ClientRestart를 오버라이드하여 대기실에서 ViewTarget 변경 차단 */
	virtual void ClientRestart_Implementation(APawn* NewPawn) override;

	/** Pawn 리플리케이션 시 엔진의 ViewTarget 자동 변경을 대기실에서 차단 */
	virtual void OnRep_Pawn() override;

	/** PlayerState 복제 도착 시점 — 클라이언트가 장착 칩을 서버에 보고하는 최초 경로 */
	virtual void OnRep_PlayerState() override;

	// 서버 → 클라이언트: 킥 당했음을 알림
	UFUNCTION(Client, Reliable)
	void ClientKicked(const FString& Reason);

	// ========== 대기실 UI ==========

	void CreateWaitingRoomUI();
	void DestroyWaitingRoomUI();
	UFUNCTION(BlueprintCallable)
	void RefreshWaitingRoomUI();

	// 서버 → 클라이언트: 대기실 UI 갱신 요청
	UFUNCTION(Client, Reliable)
	void ClientRefreshWaitingRoomUI();

	// 호스트가 Power On 클릭 시
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerRequestPowerOn();

	// 대기실 PowerOn 버튼 클릭 시 호출 (호스트=PowerOn 시도, 클라이언트=준비 토글)
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void OnPowerOnButtonPressed();

	// 클라이언트가 준비/준비 해제 토글 요청
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerToggleReady();

	// 호스트가 Kick 클릭 시
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerRequestKickPlayer(APlayerState* TargetPlayerState);

	// ========== 캐릭터 클래스 선택 ==========

	// 클라이언트에서 호출 → 서버에서 실행
	UFUNCTION(Server, Reliable, Category = "Character Selection")
	void ServerRequestChangeClass(bool bNext);

	// 블루프린트에서도 호출 가능한 래퍼
	UFUNCTION(BlueprintCallable, Category = "Character Selection")
	void RequestChangeClass(bool bNext);

	// Seamless Travel 시 PlayerState 값 유실 방지용 캐시
	EPlayerCharacterClass GetCachedSelectedClass() const { return CachedSelectedClass; }
	void SetCachedSelectedClass(EPlayerCharacterClass InClass) { CachedSelectedClass = InClass; }

	// ========== 업그레이드 칩 (Plan2.md 7.1 / 8.4) ==========

	// 로컬 세이브의 장착 목록을 서버에 보고 (로컬 컨트롤러 전용)
	void ReportUpgradeLoadout();

	// 클라이언트 신고 → 서버가 클래스 일치 확인 + 정화 후 PlayerState 에 싣는다
	UFUNCTION(Server, Reliable)
	void ServerReportUpgradeLoadout(EPlayerCharacterClass ForClass, const TArray<FName>& Chips);

	// 서버 → 클라: 스테이지 성과 통지. 클라가 로컬 세이브에 재화를 반영한다.
	UFUNCTION(Client, Reliable)
	void Client_GrantStageReward(const FDRStageRewardReport& Report);

	// 마지막으로 반영된 보상 결과 (결과창 위젯이 조회)
	UFUNCTION(BlueprintPure, Category = "Progression|Reward")
	const FDRStageRewardResult& GetLastStageRewardResult() const { return LastStageRewardResult; }

	// ========== 로비 업그레이드 화면 ==========

	// 업그레이드 장치 상호작용으로 호출 (로컬 UI — RPC 없음)
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void OpenUpgradeScreen();

	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void CloseUpgradeScreen();

	UFUNCTION(BlueprintPure, Category = "Upgrade")
	bool IsUpgradeScreenOpen() const { return bIsUpgradeScreenOpen; }

	// 화면이 ★열린/닫힌 뒤★ 불리는 BP 훅 (연출·사운드 등 부가 처리용).
	// ★위젯 생성/제거는 C++ 이 한다★ — BP 에서 CreateWidget / RemoveFromParent 하지 말 것.
	UFUNCTION(BlueprintImplementableEvent, Category = "Upgrade")
	void OnUpgradeScreenOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "Upgrade")
	void OnUpgradeScreenClosed();

	// 업그레이드 화면 위젯 클래스 (BP_DRPlayerController 에서 WBP_UpgradeScreen 지정 — 필수).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UDRUpgradeScreenWidget> UpgradeScreenWidgetClass;

	// 현재 열려 있는 화면 위젯. OpenUpgradeScreen 이 만들고 CloseUpgradeScreen 이 지운다.
	UPROPERTY(BlueprintReadOnly, Transient, Category = "UI")
	TObjectPtr<UDRUpgradeScreenWidget> UpgradeScreenWidget;

	// ========== 치트/디버그 모드 ==========

    // �׽�Ʈ�� ������ ��ŵ (��������Ʈ���� ȣ��)
    UFUNCTION(BlueprintCallable, Category = "Cheat|Phase")
    void CheatSkipToNextPhase();

	/**
	 * 치트: 업그레이드 시스템 해금 + 재화 지급. (Plan2.md 5.1 — 정식 경로는 스테이지1 최초 클리어)
	 *
	 * 콘솔(`~`)에서 `DRUnlockUpgrade` / `DRAddCurrency 5000` 으로 부른다.
	 * 세이브에 즉시 기록되므로 재시작해도 유지된다 — 되돌리려면 DRLockUpgrade 를 쓴다.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Upgrade")
	void DRUnlockUpgrade();

	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Upgrade")
	void DRAddCurrency(int32 Amount = 5000);

	// 해금 전 상태(잠김 프롬프트/차단 문구)를 다시 확인하고 싶을 때
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Upgrade")
	void DRLockUpgrade();

	/**
	 * 치트: 슬롯을 Count 개 해금한다(재화는 자동으로 채워 준다).
	 *
	 * ★칩은 슬롯을 1칸 이상 해금해야 열린다★ — IsChipUnlocked() 가
	 * UnlockedSlots >= RequiredSlotTier 로 판정하기 때문이다(DRGameInstance.cpp:651).
	 * 시스템 해금(DRUnlockUpgrade)만으로는 칩이 잠긴 채로 남는다.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Upgrade")
	void DRUnlockSlot(int32 Count = 1);

	// 현재 진행도를 로그로 덤프 (해금 수 / 재화 / 칩 상태)
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Upgrade")
	void DRDumpUpgrade();

private:
	// 치트가 조작할 대상 로봇 (업그레이드 화면의 GetViewedClass 와 같은 기준)
	EPlayerCharacterClass GetViewedUpgradeClass() const;

public:

	// ========== 카메라 피치 제한 ==========

	// 위로 바라볼 수 있는 최대 각도 (양수, 기본값 89도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0", ClampMax = "89.9"))
	float ViewPitchMax = 89.0f;

	// 아래로 바라볼 수 있는 최대 각도 (양수로 입력, 내부적으로 음수 변환됨, 기본값 89도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0", ClampMax = "89.9"))
	float ViewPitchMin = 89.0f;

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;
	virtual void ReceivedPlayer() override;
	virtual void PostSeamlessTravel() override;

	// ���� ���� �÷���
	UPROPERTY(BlueprintReadOnly, Category = "Corruption")
	bool bIsCorrupted = false;

	// ���� ���� ���� Ŭ����
	UPROPERTY(EditDefaultsOnly, Category = "UI|GameResult")
	TSubclassOf<UUserWidget> GameOverWidgetClass;

	// ���� Ŭ���� ���� Ŭ����
	UPROPERTY(EditDefaultsOnly, Category = "UI|GameResult")
	TSubclassOf<UUserWidget> GameClearWidgetClass;

	// 튜토리얼 클리어 위젯 클래스
	UPROPERTY(EditDefaultsOnly, Category = "UI|GameResult")
	TSubclassOf<UUserWidget> TutorialGameClearWidgetClass;

	// ���� ǥ�� ���� ��� ����
	UPROPERTY()
	TObjectPtr<UUserWidget> CurrentResultWidget;

	// 마지막 스테이지 보상 반영 결과 (결과창 연출용)
	UPROPERTY(BlueprintReadOnly, Category = "Progression|Reward")
	FDRStageRewardResult LastStageRewardResult;

	// 업그레이드 화면 표시 여부
	bool bIsUpgradeScreenOpen = false;

	// ========== 대기실 UI ==========

	// 대기실 카메라 캐시 (ClientRestart에서 재고정에 사용)
	UPROPERTY()
	TWeakObjectPtr<ADRWaitingRoomCameraActor> CachedWaitingRoomCamera;

	// ClientStartCameraTransitionToCharacter에서 Pawn 대기 폴백 타이머
	FTimerHandle CameraTransitionRetryHandle;

	// Pawn 복제 도착 이벤트 처리 (타이머 재시도 대체)
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn);

	// Pawn 도착 시 ViewTarget 복원 대기 여부
	bool bPendingViewTargetRestore = false;

	// Pawn 도착 시 카메라 전환 실행 대기 여부
	bool bPendingCameraTransition = false;

	// 대기실 위젯 클래스 (블루프린트에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Lobby")
	TSubclassOf<UDRWaitingRoomWidget> WaitingRoomWidgetClass;

	// 현재 대기실 위젯 인스턴스
	UPROPERTY()
	TObjectPtr<UDRWaitingRoomWidget> WaitingRoomWidget;

public:
	// 현재 대기실 위젯 반환 (Blueprint에서 설정창 등이 참조 획득용)
	UFUNCTION(BlueprintPure, Category = "UI|Lobby")
	UDRWaitingRoomWidget* GetWaitingRoomWidget() const { return WaitingRoomWidget; }

	// 해당 InputTag 키를 지금 물리적으로 누르고 있는지.
	// OverlayWidgetController 가 "GA 종료발 Released 재방송"을 억제할지 판단하는 데 쓴다 —
	// 즉발 어빌리티(GA_VacuumJetJump)는 활성화와 같은 프레임에 EndAbility 하므로,
	// 그 종료를 Released 로 그대로 방송하면 키를 누르고 있는데도 눌림 이미지가 1프레임 만에 풀린다.
	bool IsInputTagHeld(const FGameplayTag& InputTag) const { return HeldInputTags.Contains(InputTag); }

protected:

	// LobbyState 변경 시 UI 처리
	UFUNCTION()
	void OnLobbyStateChangedForUI(ELobbyState NewState);

	// ========== ��ǰ �ý��� ���� ==========

	// ����Ʈ���̽� �Ÿ�
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Part System|Config")
	float LineTraceDistance = 250.f;

	// ����Ʈ���̽� ������Ʈ ���� (��)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Part System|Config")
	float LineTraceUpdateInterval = 0.1f;

private:
	FGenericTeamId TeamId;

	// Enhanced Input System ����
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DRContext;

	// �⺻ �Է� �׼ǵ�
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	// ���� �Է� �׼�
	UPROPERTY(EditAnywhere, Category = "Input|Spectating")
	TObjectPtr<UInputAction> SpectateNextAction;

	UPROPERTY(EditAnywhere, Category = "Input|Spectating")
	TObjectPtr<UInputAction> SpectatePreviousAction;

	// ���� �޴� ��� �׼�
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ToggleSettingsAction;

	// 캐릭터 설명창 표시(Tab Hold) 입력 액션
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> CharacterInfoAction;

	// ���� �޴� ��� ó��
	void HandleToggleSettings();

	// 캐릭터 설명창 Hold 입력 처리
	void HandleCharacterInfoPressed();
	void HandleCharacterInfoReleased();

	// 튜토리얼: Tab으로 캐릭터 설명창 열림을 서버 매니저에 보고
	UFUNCTION(Server, Reliable)
	void ServerReportTutorialCharacterInfoOpened();

	// 현재 캐릭터 설명창이 표시 중인지 추적 (포커스 유실 대비)
	bool bIsCharacterInfoVisible = false;

	// Tab Hold 입력 가능한 컨텍스트인지 검사 (게임 레벨/튜토리얼이며, 대기실/설정창이 아닐 때)
	bool CanShowCharacterInfo() const;

	// ���� �Է� ó��
	void HandleSpectateNext();
	void HandleSpectatePrevious();

	// ���� �޴� ���� ����
	bool bIsSettingsMenuOpen = false;

	// ���� ��� ����
	void SetSpectateTarget(ACharacter* NewTarget);

	UFUNCTION(Server, Reliable)
	void ServerSetSpectateTarget(ACharacter* NewTarget);

	// ���� UI ������Ʈ
	UFUNCTION(Client, Reliable)
	void ClientUpdateSpectatorUI(ACharacter* SpectatedTarget);

	void UpdateSpectatorUI(ACharacter* SpectatedTarget);

	// ���� ��� ��� ó��
	UFUNCTION()
	void OnSpectatedPlayerDied(AActor* DeadActor);

	// �Է� ó�� �Լ���
	void Move(const FInputActionValue& InputActionValue);
	void Look(const FInputActionValue& InputActionValue);
	void StartJump(const FInputActionValue& InputActionValue);
	void StopJump(const FInputActionValue& InputActionValue);
	// ��ȣ�ۿ� Ű�� ������ ��
	void HandleInteract();

	// GAS �����Ƽ �Է� ó��
	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);
	void AbilityInputTagHeld(FGameplayTag InputTag);

	// �����Ƽ �Է� ����
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UDRInputConfig> InputConfig;

	UPROPERTY()
	TObjectPtr<UDRAbilitySystemComponent> DRAbilitySystemComponent;

	UDRAbilitySystemComponent* GetASC() const;

	// 해당 InputTag 의 어빌리티가 지금 차단 상태인지 (Carrying 중이거나 BlockedAbilityTags 에 걸린 상태)
	bool IsAbilityInputBlocked(const FGameplayTag& InputTag) const;

	// 현재 눌려 있는 스킬 InputTag 집합 (IsInputTagHeld 의 백킹 데이터).
	// Pressed/Released 최상단에서 갱신하고, 폰 교체 시 비운다 (키를 누른 채 폰이 바뀌면 Released 가 유실될 수 있음)
	TSet<FGameplayTag> HeldInputTags;

	// 이 컨트롤러가 리슨 서버의 호스트인지 (서버에서 실행 중인 로컬 컨트롤러)
	bool IsListenServerHost() const { return HasAuthority() && IsLocalController(); }

	// 마우스 감도 캐시 (Look()이 매 입력마다 Subsystem 체인을 타지 않도록)
	float CachedMouseSensitivity = 1.0f;

	UFUNCTION()
	void HandleMouseSensitivityChanged(float NewSensitivity);

	// 레벨 컨텍스트 캐시 (Unknown이면 다음 조회 때 재판별 - GameState 복제 지연 대비)
	mutable EDRLevelContext CachedLevelContext = EDRLevelContext::Unknown;

	// 캐시된 레벨 컨텍스트 조회
	EDRLevelContext GetLevelContext() const;

	// 맵 이름/GameState 타입으로 현재 레벨 컨텍스트 판별
	EDRLevelContext DetermineLevelContext() const;

	// ������ �ؽ�Ʈ ������Ʈ Ŭ����
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UDamageTextComponent> DamageTextComponentClass;

	// 표시 중인 데미지 텍스트 추적 (BP 애니메이션 종료 시 자체 파괴되므로 위크 포인터)
	TArray<TWeakObjectPtr<UDamageTextComponent>> ActiveDamageTexts;

	// 데미지 텍스트 동시 표시 상한 (AoE 다중 타격 시 컴포넌트 생성 스파이크 방지)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Damage")
	int32 MaxConcurrentDamageTexts = 30;

	// Seamless Travel 시 캐릭터 클래스 보존 (PlayerController는 Travel에서 생존)
	EPlayerCharacterClass CachedSelectedClass = EPlayerCharacterClass::Gardener;

	// ����Ʈ���̽� Ȱ��ȭ ����
	bool bPartDetectionEnabled = false;

	// 사이트 라인트레이스 감지 활성화 여부 (사이트 박스 오버랩 중일 때 true)
	bool bSiteDetectionEnabled = false;

	// 박스 오버랩 중인 사이트 집합 (감지 후보, 여러 사이트 동시 오버랩 대응)
	TSet<TWeakObjectPtr<AActor>> OverlappedSites;

	// 청소기 탑승 감지 활성화 여부 (MountDetectionSphere 오버랩 중일 때 true)
	bool bMountDetectionEnabled = false;

	// 오버랩 중인 청소기 집합 (탑승 감지 후보)
	TSet<TWeakObjectPtr<AActor>> OverlappedMounts;

	// 현재 감지(라인트레이스) 중인 청소기 (IDRInteractable 구현 액터)
	UPROPERTY()
	TObjectPtr<AActor> CurrentDetectedMount;

	// ========== 상호작용 감지 공통 처리 (Part/Site 공용) ==========

	// 오버랩 집합 등록/해제 + 감지 플래그 갱신 + 이탈 시 UI 정리
	void SetInteractableDetectionEnabled(bool bEnabled, AActor* Interactable,
		TSet<TWeakObjectPtr<AActor>>& OverlapSet, bool& bDetectionFlag, TObjectPtr<AActor>& CurrentDetected);

	// 현재 감지 대상 전환 + 상호작용 UI 위젯 토글 (IDRInteractable)
	void SetCurrentDetectedInteractable(AActor* NewDetected, TObjectPtr<AActor>& CurrentDetected);

	// 사이트 감지 변화를 조준선 UI(OverlayWidgetController)에 전달 (로컬 전용)
	void NotifyCrosshairSiteDetected(bool bDetected);

	// ���� �÷��̾ �׾����� ����

	// ����Ʈ���̽� Ÿ�̸�
	float LineTraceTimer = 0.f;

	// 탑승 요 동기화 전송 간격 타이머 / 마지막 전송 값 (클라 전용)
	float MountedYawSyncTimer = 0.f;
	float LastSentMountedYaw = 0.f;

	// ���� ��ó�� �ִ� ��ǰ
	TSet<TWeakObjectPtr<AActor>> OverlappedParts;

	// 현재 감지(라인트레이스) 중인 부품 (IDRInteractable 구현 액터)
	UPROPERTY()
	TObjectPtr<AActor> CurrentDetectedPart;

	// ========== 스테이지2 상호작용 프롭 (Plan6 §5.5-b) ==========
	// 레버/버튼/단말 등 조작 대상을 하나의 슬롯 + 하나의 분기로 처리한다.

	UPROPERTY()
	TObjectPtr<AActor> CurrentDetectedProp;

	ADRS2InteractProp* FindPropByLineTrace();

	// ========== 열차 좌석 (Plan6 §5.5) ==========

	UPROPERTY()
	TObjectPtr<AActor> CurrentDetectedCar;

	class ADRS2TrainCar* FindTrainCarByLineTrace();

	// 점프 키로 하차 요청 (마운트 하차와 동일 관례)
	UFUNCTION(Server, Reliable)
	void ServerRequestTrainDeboard();

public:
	// 8퍼즐 UI 열기 (서버 -> 요청한 클라이언트)
	UFUNCTION(Client, Reliable)
	void Client_OpenSlidePuzzleUI(ADRS2SlidePuzzle* Puzzle);

	// HUD/BP 가 바인딩해 실제 위젯을 생성한다
	UPROPERTY(BlueprintAssignable, Category = "S2|Puzzle")
	FOnSlidePuzzleUIRequested OnSlidePuzzleUIRequested;

	// 원래의 private 구역으로 복귀 (이 아래 멤버들의 접근 수준을 바꾸지 않기 위함)
private:

	// ��ǰ ��� ��û
	UFUNCTION(Server, Reliable)
	void ServerRequestDropPart();
	
	// FreeRoam 진입 시 HUD 오버레이 초기화 (대기실에서 스킵된 경우)
	void InitOverlayForFreeRoam();

	// �������� ������ ��ŵ ����
    UFUNCTION(Server, Reliable)
    void ServerCheatSkipToNextPhase();
};
