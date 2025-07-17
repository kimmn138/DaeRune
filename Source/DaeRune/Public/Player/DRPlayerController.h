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
 * ADRPlayerController
 *
 * 플레이어 입력 처리 및 데미지 피드백 관리 컨트롤러 클래스임
 */
UCLASS()
class DAERUNE_API ADRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADRPlayerController();

	/**
	 * ShowDamageNumber
	 *
	 * 클라이언트에서 데미지 숫자 표시 RPC 선언임
	 */
	UFUNCTION(Client, Reliable)
	void ShowDamageNumber(float DamageAmount, ACharacter* TargetCharacter);

protected:
	// 플레이어 컨트롤러 시작 시 호출 재정의 함수임
	virtual void BeginPlay() override;

	// 입력 컴포넌트 설정 재정의 함수임
	virtual void SetupInputComponent() override;

private:
	// Enhanced Input 매핑 컨텍스트 멤버임
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DRContext;

	// 이동 입력 액션 참조임
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	// 시점 변경 입력 액션 참조임
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	// 이동 로직 처리 함수임
	void Move(const FInputActionValue& InputActionValue);

	// 시점 변경 로직 처리 함수임
	void Look(const FInputActionValue& InputActionValue);

	// 능력 입력 시작 처리 함수임
	void AbilityInputTagPressed(FGameplayTag InputTag);
	// 능력 입력 해제 처리 함수임
	void AbilityInputTagReleased(FGameplayTag InputTag);
	// 능력 입력 유지 처리 함수임
	void AbilityInputTagHeld(FGameplayTag InputTag);

	// 입력 설정 데이터 참조임
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UDRInputConfig> InputConfig;

	// DR 전용 ASC 캐시 멤버임
	UPROPERTY()
	TObjectPtr<UDRAbilitySystemComponent> DRAbilitySystemComponent;

	// ASC 반환 헬퍼 함수 선언임
	UDRAbilitySystemComponent* GetASC();

	// 데미지 텍스트 컴포넌트 클래스 참조임
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UDamageTextComponent> DamageTextComponentClass;
};
