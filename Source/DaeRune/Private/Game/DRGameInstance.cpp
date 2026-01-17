// Copyright DaeRune


#include "Game/DRGameInstance.h"

void UDRGameInstance::Init()
{
    Super::Init();

    // 네트워크 실패 델리게이트 바인딩
    GEngine->OnNetworkFailure().AddUObject(this, &UDRGameInstance::HandleNetworkFailure);
}

void UDRGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
    // 메인 메뉴로 이동
    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        PC->ClientTravel(TEXT("/Game/Maps/MainMenu"), ETravelType::TRAVEL_Absolute);
    }
}
