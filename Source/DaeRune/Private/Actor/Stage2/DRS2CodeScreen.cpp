// Copyright DaeRune

#include "Actor/Stage2/DRS2CodeScreen.h"

ADRS2CodeScreen::ADRS2CodeScreen()
{
	PrimaryActorTick.bCanEverTick = false;

	// 표시 전용. 상태는 상위 퍼즐/금고가 복제하고 이 액터는 결과만 받아 그린다.
	bReplicates = false;

	ScreenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenMesh"));
	SetRootComponent(ScreenMesh);
	ScreenMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRS2CodeScreen::SetDigits(const TArray<int32>& InDigits)
{
	Digits = InDigits;
	OnDigitsChanged(Digits);
}

void ADRS2CodeScreen::ClearDigits()
{
	Digits.Reset();
	OnDigitsChanged(Digits);
}
