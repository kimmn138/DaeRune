// Copyright DaeRune

#include "Actor/Stage2/DRS2CodeScreen.h"

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

	const int32 SlotTotal = ScreenMesh->GetNumMaterials();

	for (int32 Slot : DigitMaterialSlots)
	{
		if (Slot < 0 || Slot >= SlotTotal)
		{
			UE_LOG(LogDR, Error,
				TEXT("[S2Screen] %s: 머티리얼 슬롯 %d 이 범위(0~%d) 밖입니다."),
				*GetName(), Slot, SlotTotal - 1);
			DisplayMIDs.Add(nullptr);
			continue;
		}

		UMaterialInstanceDynamic* MID = ScreenMesh->CreateAndSetMaterialInstanceDynamic(Slot);
		if (!MID)
		{
			UE_LOG(LogDR, Error,
				TEXT("[S2Screen] %s: 슬롯 %d 의 MID 생성 실패. 그 슬롯에 머티리얼이 있는지 확인하세요."),
				*GetName(), Slot);
		}

		DisplayMIDs.Add(MID);
	}

	if (DigitTextures.Num() < 10)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[S2Screen] %s: DigitTextures 가 %d개뿐입니다. 0~9 를 채우세요."),
			*GetName(), DigitTextures.Num());
	}
}

UTexture2D* ADRS2CodeScreen::DigitToTexture(int32 Digit) const
{
	if (!DigitTextures.IsValidIndex(Digit))
	{
		return EmptySlotTexture.Get();
	}

	UTexture2D* Texture = DigitTextures[Digit].Get();
	return Texture ? Texture : EmptySlotTexture.Get();
}

void ADRS2CodeScreen::ApplyDigitTextures()
{
	for (int32 SlotIndex = 0; SlotIndex < DisplayMIDs.Num(); ++SlotIndex)
	{
		UMaterialInstanceDynamic* MID = DisplayMIDs[SlotIndex];
		if (!MID) continue;

		// 배열보다 자리가 많으면 남는 자리는 미표시로 둔다.
		const int32 Digit = Digits.IsValidIndex(SlotIndex) ? Digits[SlotIndex] : INDEX_NONE;

		MID->SetTextureParameterValue(TextureParameterName, DigitToTexture(Digit));
	}
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
