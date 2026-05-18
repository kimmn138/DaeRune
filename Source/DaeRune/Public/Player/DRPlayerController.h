// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "GenericTeamAgentInterface.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
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
class ADRWaitingRoomCameraActor;
class UDRWaitingRoomWidget;
enum class ELobbyState : uint8;

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
	UFUNCTION(Client, Reliable)
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

	UFUNCTION(Server, Reliable)
	void ServerNotifyLineTraceDetected(ADRCleanserPart* Part);

	UFUNCTION(Server, Reliable)
	void ServerNotifyLineTraceLost(ADRCleanserPart* Part);

	UPROPERTY()
	TObjectPtr<ADRCleanserSite> CurrentOverlappedSite;

	// 사이트 감지 활성화/비활성화 (사이트 박스 오버랩에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Part System")
	void SetSiteDetectionEnabled(bool bEnabled, ADRCleanserSite* Site);

	// 시점 라인트레이스로 사이트 찾기
	UFUNCTION(BlueprintCallable, Category = "Part System")
	ADRCleanserSite* FindSiteByLineTrace();

	UFUNCTION(Server, Reliable)
	void ServerNotifySiteDetected(ADRCleanserSite* Site);

	UFUNCTION(Server, Reliable)
	void ServerNotifySiteLost(ADRCleanserSite* Site);

	UFUNCTION(Server, Reliable)
	void ServerRequestInstallPartToSite(ADRCleanserSite* Site);

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

	// ========== 치트/디버그 모드 ==========

    // �׽�Ʈ�� ������ ��ŵ (��������Ʈ���� ȣ��)
    UFUNCTION(BlueprintCallable, Category = "Cheat|Phase")
    void CheatSkipToNextPhase();

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

	// ���� ǥ�� ���� ��� ����
	UPROPERTY()
	TObjectPtr<UUserWidget> CurrentResultWidget;

	// ========== 대기실 UI ==========

	// 대기실 카메라 캐시 (ClientRestart에서 재고정에 사용)
	UPROPERTY()
	TWeakObjectPtr<ADRWaitingRoomCameraActor> CachedWaitingRoomCamera;

	// ClientStartCameraTransitionToCharacter에서 Pawn 대기용 재시도 타이머
	FTimerHandle CameraTransitionRetryHandle;

	// 대기실 위젯 클래스 (블루프린트에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Lobby")
	TSubclassOf<UDRWaitingRoomWidget> WaitingRoomWidgetClass;

	// 현재 대기실 위젯 인스턴스
	UPROPERTY()
	TObjectPtr<UDRWaitingRoomWidget> WaitingRoomWidget;

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

	UDRAbilitySystemComponent* GetASC();

	// 해당 InputTag 의 어빌리티가 지금 차단 상태인지 (Carrying 중이거나 BlockedAbilityTags 에 걸린 상태)
	bool IsAbilityInputBlocked(const FGameplayTag& InputTag) const;

	// ������ �ؽ�Ʈ ������Ʈ Ŭ����
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UDamageTextComponent> DamageTextComponentClass;

	// Seamless Travel 시 캐릭터 클래스 보존 (PlayerController는 Travel에서 생존)
	EPlayerCharacterClass CachedSelectedClass = EPlayerCharacterClass::GardenRobot;

	// ����Ʈ���̽� Ȱ��ȭ ����
	bool bPartDetectionEnabled = false;

	// 사이트 라인트레이스 감지 활성화 여부 (사이트 박스 오버랩 중일 때 true)
	bool bSiteDetectionEnabled = false;

	// 박스 오버랩 중인 사이트 (감지 후보)
	UPROPERTY()
	TObjectPtr<ADRCleanserSite> NearbySite;

	// ���� �÷��̾ �׾����� ����

	// ����Ʈ���̽� Ÿ�̸�
	float LineTraceTimer = 0.f;

	// ���� ��ó�� �ִ� ��ǰ
	UPROPERTY()
	TObjectPtr<ADRCleanserPart> NearbyPart;

	// ���� ������ ��ǰ
	UPROPERTY()
	TObjectPtr<ADRCleanserPart> CurrentDetectedPart;

	// ��ǰ ȹ�� ��û
	UFUNCTION(Server, Reliable)
	void ServerRequestPickupPart(ADRCleanserPart* Part);

	// ��ǰ ��� ��û
	UFUNCTION(Server, Reliable)
	void ServerRequestDropPart();
	
	// FreeRoam 진입 시 HUD 오버레이 초기화 (대기실에서 스킵된 경우)
	void InitOverlayForFreeRoam();

	// �������� ������ ��ŵ ����
    UFUNCTION(Server, Reliable)
    void ServerCheatSkipToNextPhase();
};
