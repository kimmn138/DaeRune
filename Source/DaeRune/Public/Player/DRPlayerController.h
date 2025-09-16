// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "DRPlayerController.generated.h"

class UDamageTextComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UDRInputConfig;
class UDRAbilitySystemComponent;

/**
 * DaeRune 플레이어의 입력 처리 및 UI 관리 클래스
 */
UCLASS()
class DAERUNE_API ADRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADRPlayerController();

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

protected:
	virtual void BeginPlay() override;

	virtual void SetupInputComponent() override;

	// 부패 상태 플래그
	UPROPERTY(BlueprintReadOnly, Category = "Corruption")
	bool bIsCorrupted = false;

private:
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

	// 입력 처리 함수들
	void Move(const FInputActionValue& InputActionValue);
	void Look(const FInputActionValue& InputActionValue);
	void StartJump(const FInputActionValue& InputActionValue);
	void StopJump(const FInputActionValue& InputActionValue);

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
};
