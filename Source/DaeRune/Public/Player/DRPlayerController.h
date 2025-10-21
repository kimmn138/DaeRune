// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"
#include "DRPlayerController.generated.h"

// 상호작용 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractPressed);

class UDamageTextComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UDRInputConfig;
class UDRAbilitySystemComponent;
class ADRCleanserPart;

/**
 * DaeRune 플레이어의 입력 처리 및 UI 관리 클래스
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

	// 데미지 수치 표시
	UFUNCTION(Client, Reliable)
	void ShowDamageNumber(float DamageAmount, ACharacter* TargetCharacter);

	// 부패 상태 변경 처리
	UFUNCTION(BlueprintCallable, Category = "Corruption")
	void OnCorruptedStateChanged(bool bIsStateChanged);

	// 음성 채팅 활성화/비활성화
	UFUNCTION(BlueprintImplementableEvent, Category = "Corruption")
	void SetVoiceChatEnabled(bool bEnabled);

	// 아군/적 구분 표시 변경
	UFUNCTION(BlueprintImplementableEvent, Category = "Corruption")
	void SetTeamVisualsEnabled(bool bEnabled);

	// 부패 상태 확인
	UFUNCTION(BlueprintCallable, Category = "Corruption")
	bool IsInCorruptedState() const { return bIsCorrupted; }

	// 상호작용 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Input")
	FOnInteractPressed OnInteractPressed;

	// ========== 부품 시스템 ==========

	// 부품 감지 활성화/비활성화
	UFUNCTION(BlueprintCallable, Category = "Part System")
	void SetPartDetectionEnabled(bool bEnabled, class ADRCleanserPart* Part);

	// 라인트레이싱으로 부품 찾기
	UFUNCTION(BlueprintCallable, Category = "Part System")
	ADRCleanserPart* FindPartByLineTrace();

	// 부품 획득 UI 표시 여부
	UFUNCTION(BlueprintImplementableEvent, Category = "Part System")
	void ShowPartPickupUI();

	UFUNCTION(BlueprintImplementableEvent, Category = "Part System")
	void HidePartPickupUI();

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	// 부패 상태 플래그
	UPROPERTY(BlueprintReadOnly, Category = "Corruption")
	bool bIsCorrupted = false;

	// ========== 부품 시스템 설정 ==========

	// 라인트레이싱 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Part System|Config")
	float LineTraceDistance = 100.f;

	// 라인트레이싱 업데이트 간격 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Part System|Config")
	float LineTraceUpdateInterval = 0.1f;

private:
	FGenericTeamId TeamId;

	// Enhanced Input System 설정
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DRContext;

	// 기본 입력 액션들
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	// 입력 처리 함수들
	void Move(const FInputActionValue& InputActionValue);
	void Look(const FInputActionValue& InputActionValue);
	void StartJump(const FInputActionValue& InputActionValue);
	void StopJump(const FInputActionValue& InputActionValue);
	// 상호작용 키를 눌렀을 때
	void HandleInteract();

	// GAS 어빌리티 입력 처리
	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);
	void AbilityInputTagHeld(FGameplayTag InputTag);

	// 어빌리티 입력 설정
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UDRInputConfig> InputConfig;

	UPROPERTY()
	TObjectPtr<UDRAbilitySystemComponent> DRAbilitySystemComponent;

	UDRAbilitySystemComponent* GetASC();

	// 데미지 텍스트 컴포넌트 클래스
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UDamageTextComponent> DamageTextComponentClass;

	// 라인트레이싱 활성화 여부
	bool bPartDetectionEnabled = false;

	// 라인트레이싱 타이머
	float LineTraceTimer = 0.f;

	// 현재 근처에 있는 부품
	UPROPERTY()
	TObjectPtr<ADRCleanserPart> NearbyPart;

	// 현재 감지된 부품
	UPROPERTY()
	TObjectPtr<ADRCleanserPart> CurrentDetectedPart;

	// 부품 획득 요청 (서버 RPC)
	UFUNCTION(Server, Reliable)
	void ServerRequestPickupPart(ADRCleanserPart* Part);
};
