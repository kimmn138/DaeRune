// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRUserWidget.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRUserWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void SetWidgetController(UObject* InWidgetController);

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UObject> WidgetController;

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void WidgetControllerSet();
};
