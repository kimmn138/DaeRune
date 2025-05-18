// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DRPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * 
 */
UCLASS()
class DAERUNE_API ADRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADRPlayerController();

protected:
	virtual void BeginPlay() override;

	virtual void SetupInputComponent() override;

private:
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DRContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	void Move(const FInputActionValue& InputActionValue);
};
