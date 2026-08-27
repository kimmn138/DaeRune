// Copyright DaeRune

#include "Actor/Stage2/DRS2CodeScreen.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "DaeRune/DRLogChannels.h"

ADRS2CodeScreen::ADRS2CodeScreen()
{
	PrimaryActorTick.bCanEverTick = false;

	// 표시 전용. 상태는 상위 퍼즐/금고가 복제하고 이 액터는 결과만 받아 그린다.
	bReplicates = false;

	ScreenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenMesh"));
	SetRootComponent(ScreenMesh);
	ScreenMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRS2CodeScreen::BeginPlay()
{
	Super::BeginPlay();

	EnsureDigitMIDs();

	// 아직 SetDigits 가 오기 전이면 전 자리를 "미입력" 으로 둔다.
	RefreshDigitTextures();
}

void ADRS2CodeScreen::EnsureDigitMIDs()
{
	DigitMIDs.Reset();

	if (DigitMaterialSlots.Num() > 0)
	{
		// ★기본 방식: 한 메시의 머티리얼 슬롯 여러 개를 자리로 쓴다.
		BuildMIDsFromMaterialSlots();
	}
	else
	{
		// 대안: 자리마다 별도 메시 컴포넌트를 둔 경우
		BuildMIDsFromTaggedComponents();
	}

	if (DigitMIDs.Num() == 0)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[S2Screen] %s: 표시할 자리를 찾지 못했습니다. DigitMaterialSlots 를 채우거나 '%s' 태그를 다세요."),
			*GetName(), *DigitComponentTag.ToString());
	}

	if (DigitTextures.Num() < 10)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[S2Screen] %s: DigitTextures 가 %d개뿐입니다. 0~9 를 채우세요."),
			*GetName(), DigitTextures.Num());
	}
}

void ADRS2CodeScreen::BuildMIDsFromMaterialSlots()
{
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
			DigitMIDs.Add(nullptr);
			continue;
		}

		UMaterialInstanceDynamic* MID = ScreenMesh->CreateAndSetMaterialInstanceDynamic(Slot);
		if (!MID)
		{
			UE_LOG(LogDR, Error,
				TEXT("[S2Screen] %s: 슬롯 %d 의 MID 생성 실패. 그 슬롯에 머티리얼이 있는지 확인하세요."),
				*GetName(), Slot);
		}

		DigitMIDs.Add(MID);
	}
}

void ADRS2CodeScreen::BuildMIDsFromTaggedComponents()
{
	TArray<UStaticMeshComponent*> Found;
	GetComponents<UStaticMeshComponent>(Found);

	TArray<UStaticMeshComponent*> DigitMeshes;
	for (UStaticMeshComponent* Component : Found)
	{
		if (Component && Component->ComponentHasTag(DigitComponentTag))
		{
			DigitMeshes.Add(Component);
		}
	}

	// 이름 순 정렬이 곧 자릿수 순서(왼쪽부터)다.
	DigitMeshes.Sort([](const UStaticMeshComponent& A, const UStaticMeshComponent& B)
	{
		return A.GetName() < B.GetName();
	});

	for (UStaticMeshComponent* Mesh : DigitMeshes)
	{
		UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(FallbackMaterialSlot);
		if (!MID)
		{
			UE_LOG(LogDR, Error,
				TEXT("[S2Screen] %s / %s: 슬롯 %d 의 MID 생성 실패."),
				*GetName(), *Mesh->GetName(), FallbackMaterialSlot);
		}

		DigitMIDs.Add(MID);
	}
}

UTexture2D* ADRS2CodeScreen::DigitToTexture(int32 Digit) const
{
	// 음수(미공개/미입력)이거나 텍스처가 없으면 빈 자리 표시
	if (!DigitTextures.IsValidIndex(Digit))
	{
		return EmptySlotTexture.Get();
	}

	UTexture2D* Texture = DigitTextures[Digit].Get();
	return Texture ? Texture : EmptySlotTexture.Get();
}

void ADRS2CodeScreen::RefreshDigitTextures()
{
	for (int32 SlotIndex = 0; SlotIndex < DigitMIDs.Num(); ++SlotIndex)
	{
		UMaterialInstanceDynamic* MID = DigitMIDs[SlotIndex];
		if (!MID) continue;

		// 배열보다 자리가 많으면 남는 자리는 빈 표시로 둔다.
		const int32 Digit = Digits.IsValidIndex(SlotIndex) ? Digits[SlotIndex] : INDEX_NONE;

		MID->SetTextureParameterValue(TextureParameterName, DigitToTexture(Digit));
	}
}

void ADRS2CodeScreen::SetDigits(const TArray<int32>& InDigits)
{
	Digits = InDigits;

	RefreshDigitTextures();
	OnDigitsChanged(Digits);
}

void ADRS2CodeScreen::ClearDigits()
{
	Digits.Reset();

	RefreshDigitTextures();
	OnDigitsChanged(Digits);
}
