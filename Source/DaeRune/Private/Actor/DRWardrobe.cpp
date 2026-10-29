// Copyright DaeRune


#include "Actor/DRWardrobe.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "Game/DRGameInstance.h"
#include "Kismet/GameplayStatics.h"

ADRWardrobe::ADRWardrobe()
{
	WardrobeMesh = CreateStationRootMesh(TEXT("WardrobeMesh"));
}

bool ADRWardrobe::HasAnySkinAvailable() const
{
	const UDRGameInstance* GI = Cast<UDRGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (!GI) return false;

	// "지금 고른 로봇"의 옷만 본다 (옷장 화면의 GetViewedClass 와 같은 기준)
	EPlayerCharacterClass ViewedClass = EPlayerCharacterClass::Gardener;
	if (OverlappingLocalController)
	{
		if (const ADRPlayerState* PS = OverlappingLocalController->GetPlayerState<ADRPlayerState>())
		{
			ViewedClass = PS->GetSelectedPlayerClass();
		}
	}

	return GI->HasAnySkinAvailable(ViewedClass);
}

void ADRWardrobe::OpenStationScreen(ADRPlayerController* PC)
{
	PC->OpenWardrobeScreen();
}
