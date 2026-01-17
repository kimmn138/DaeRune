// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DRGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	virtual void Init() override;

private:
	// 네트워크 에러 핸들러
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
};
