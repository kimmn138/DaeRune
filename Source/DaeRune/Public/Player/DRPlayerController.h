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
 * 
 */
UCLASS()
class DAERUNE_API ADRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADRPlayerController();

	UFUNCTION(Client, Reliable)
	void ShowDamageNumber(float DamageAmount, ACharacter* TargetCharacter);

protected:
	virtual void BeginPlay() override;

	virtual void SetupInputComponent() override;

private:
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DRContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	void Move(const FInputActionValue& InputActionValue);

	void Look(const FInputActionValue& InputActionValue);

	void StartJump(const FInputActionValue& InputActionValue);
	void StopJump(const FInputActionValue& InputActionValue);

	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);
	void AbilityInputTagHeld(FGameplayTag InputTag);

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UDRInputConfig> InputConfig;

	UPROPERTY()
	TObjectPtr<UDRAbilitySystemComponent> DRAbilitySystemComponent;

	UDRAbilitySystemComponent* GetASC();

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UDamageTextComponent> DamageTextComponentClass;
};
