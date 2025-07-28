// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DRHUD.generated.h"

class UAttributeSet;
class UAbilitySystemComponent;
class UOverlayWidgetController;
class UDRUserWidget;
struct FWidgetControllerParams;

/**
 * 
 */
UCLASS()
class DAERUNE_API ADRHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	UOverlayWidgetController* GetOverlayWidgetController(const FWidgetControllerParams& WCParams);

	void InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS);

protected:

private:
	UPROPERTY()
	TObjectPtr<UDRUserWidget>  OverlayWidget;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UDRUserWidget> OverlayWidgetClass;

	UPROPERTY()
	TObjectPtr<UOverlayWidgetController> OverlayWidgetController;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UOverlayWidgetController> OverlayWidgetControllerClass;
};
