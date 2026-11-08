// Copyright DaeRune


#include "Actor/DRUpgradeStation.h"
#include "Player/DRPlayerController.h"
#include "Game/DRGameInstance.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "DRInteraction"

ADRUpgradeStation::ADRUpgradeStation()
{
	StationMesh = CreateStationRootMesh(TEXT("StationMesh"));

	// 화면 상호작용 안내 — 업그레이드는 스테이지1 최초 클리어로 해금된다 (Plan2.md 5.1)
	PromptText = LOCTEXT("UpgradeStation", "업그레이드하기");
	UnavailablePromptText = LOCTEXT("UpgradeStationLocked", "스테이지 1을 클리어하면 열립니다");
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

#undef LOCTEXT_NAMESPACE
