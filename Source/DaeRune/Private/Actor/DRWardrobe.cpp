// Copyright DaeRune


#include "Actor/DRWardrobe.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "Game/DRGameInstance.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "DRInteraction"

ADRWardrobe::ADRWardrobe()
{
	WardrobeMesh = CreateStationRootMesh(TEXT("WardrobeMesh"));

	// 화면 상호작용 안내 — 이 로봇의 옷이 하나도 없으면 열지 않는다 (CanUseStation)
	PromptText = LOCTEXT("Wardrobe", "옷장 열기");
	UnavailablePromptText = LOCTEXT("WardrobeNoSkins", "옷장 준비 중입니다");
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

#undef LOCTEXT_NAMESPACE
