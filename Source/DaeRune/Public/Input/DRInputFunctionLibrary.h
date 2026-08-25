// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRInputFunctionLibrary.generated.h"

UCLASS()
class DAERUNE_API UDRInputFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Input|Mouse")
	static bool IsMouseButtonPressed(FKey Button);
};
