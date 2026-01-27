// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "GenericTeamAgentInterface.h"
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

	// ���� ä�� Ȱ��ȭ/��Ȱ��ȭ
	UFUNCTION(BlueprintImplementableEvent, Category = "Corruption")
	void SetVoiceChatEnabled(bool bEnabled);

	// �Ʊ�/�� ���� ǥ�� ����
	UFUNCTION(BlueprintImplementableEvent, Category = "Corruption")
	void SetTeamVisualsEnabled(bool bEnabled);

	// ���� ���� Ȯ��
	UFUNCTION(BlueprintCallable, Category = "Corruption")
	bool IsInCorruptedState() const { return bIsCorrupted; }

	// ���� ä�� ������Ʈ
	UFUNCTION(BlueprintCallable, Category = "Voice Chat")
	void UpdateVoiceChannelForDeathState(bool bIsDead);

	// Ư�� �÷��̾� ��Ʈ/���Ʈ
	UFUNCTION(BlueprintCallable, Category = "Voice Chat")
	void SetPlayerVoiceMuted(APlayerState* TargetPlayer, bool bMute);

	// ��� �÷��̾� ���� ��Ʈ ���� ������Ʈ
	void RefreshAllPlayerVoiceMutes();

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
	
	// ========== ġƮ/����� ��� ==========
    	
    // �׽�Ʈ�� ������ ��ŵ (��������Ʈ���� ȣ��)
    UFUNCTION(BlueprintCallable, Category = "Cheat|Phase")
    void CheatSkipToNextPhase();

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

	// ========== ��ǰ �ý��� ���� ==========

	// ����Ʈ���̽� �Ÿ�
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Part System|Config")
	float LineTraceDistance = 100.f;

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

	// ���� �޴� ��� ó��
	void HandleToggleSettings();

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

	UPROPERTY()
	TObjectPtr<class UDRSettingsWidget> SettingsWidget;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UDRSettingsWidget> SettingsWidgetClass;

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

	// ������ �ؽ�Ʈ ������Ʈ Ŭ����
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UDamageTextComponent> DamageTextComponentClass;

	// ����Ʈ���̽� Ȱ��ȭ ����
	bool bPartDetectionEnabled = false;

	// ���� �÷��̾ �׾����� ����
	bool bIsDeadForVoice = false;

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
	
	// �������� ������ ��ŵ ����
    UFUNCTION(Server, Reliable)
    void ServerCheatSkipToNextPhase();
};
