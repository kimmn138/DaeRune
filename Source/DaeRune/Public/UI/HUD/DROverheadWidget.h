// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DROverheadWidget.generated.h"

class UTextBlock;

/**
 * 
 */
UCLASS()
class DAERUNE_API UDROverheadWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* DisplayText;

	UFUNCTION(BlueprintCallable)
	void SetDisplayText(FString TextToDisplay);

protected:
	virtual void NativeDestruct() override;
};
