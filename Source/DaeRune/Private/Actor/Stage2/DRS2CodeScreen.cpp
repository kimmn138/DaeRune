// Copyright DaeRune

#include "Actor/Stage2/DRS2CodeScreen.h"
#include "Actor/Stage2/DRS2DigitDisplay.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "DaeRune/DRLogChannels.h"

ADRS2CodeScreen::ADRS2CodeScreen()
{
	PrimaryActorTick.bCanEverTick = false;

	// 표시 전용. 상태는 상위 퍼즐이 복제하고 이 액터는 결과만 받아 그린다.
	bReplicates = false;

	ScreenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenMesh"));
	SetRootComponent(ScreenMesh);
	ScreenMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRS2CodeScreen::BeginPlay()
{
	Super::BeginPlay();

	EnsureDisplayMIDs();

	// ★퍼즐이 자기 BeginPlay 에서 SetDigits({-1}) 을 먼저 보냈을 수 있다.
	//   그때는 MID 가 아직 없어 반영되지 않았으므로 여기서 한 번 더 적용한다.
	ApplyDigitTextures();
}

void ADRS2CodeScreen::EnsureDisplayMIDs()
{
	DisplayMIDs.Reset();

	if (DigitMaterialSlots.Num() == 0)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[S2Screen] %s: DigitMaterialSlots 가 비어 있습니다. 숫자가 표시되지 않습니다."),
			*GetName());
		return;
	}

	if (!ScreenMesh || !ScreenMesh->GetStaticMesh())
	{
		UE_LOG(LogDR, Error, TEXT("[S2Screen] %s: ScreenMesh 에 메시가 지정되지 않았습니다."), *GetName());
		return;
	}

	DRS2DigitDisplay::BuildMIDs(ScreenMesh, DigitMaterialSlots, DigitTextures.Num(),
		FString::Printf(TEXT("[S2Screen] %s:"), *GetName()), DisplayMIDs);
}

void ADRS2CodeScreen::ApplyDigitTextures()
{
	DRS2DigitDisplay::ApplyDigits(DisplayMIDs, TextureParameterName, DigitTextures, EmptySlotTexture, Digits);
}

void ADRS2CodeScreen::SetDigits(const TArray<int32>& InDigits)
{
	Digits = InDigits;

	ApplyDigitTextures();
	OnDigitsChanged(Digits);
}

void ADRS2CodeScreen::ClearDigits()
{
	Digits.Reset();

	ApplyDigitTextures();
	OnDigitsChanged(Digits);
}
