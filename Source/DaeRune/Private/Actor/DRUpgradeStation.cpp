// Copyright DaeRune


#include "Actor/DRUpgradeStation.h"
#include "Player/DRPlayerController.h"
#include "Game/DRGameInstance.h"
#include "Kismet/GameplayStatics.h"

ADRUpgradeStation::ADRUpgradeStation()
{
	StationMesh = CreateStationRootMesh(TEXT("StationMesh"));
}

bool ADRUpgradeStation::IsUpgradeSystemUnlocked() const
{
	const UDRGameInstance* GI = Cast<UDRGameInstance>(UGameplayStatics::GetGameInstance(this));
	return GI ? GI->IsUpgradeSystemUnlocked() : false;
}

void ADRUpgradeStation::OpenStationScreen(ADRPlayerController* PC)
{
	PC->OpenUpgradeScreen();
}
