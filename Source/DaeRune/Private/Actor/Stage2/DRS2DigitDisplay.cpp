// Copyright DaeRune


#include "Actor/Stage2/DRS2DigitDisplay.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DaeRune/DRLogChannels.h"

namespace DRS2DigitDisplay
{
	void BuildMIDs(UStaticMeshComponent* Mesh, const TArray<int32>& Slots, int32 NumDigitTextures,
		const FString& LogOwner, TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutMIDs)
	{
		OutMIDs.Reset();
		if (!Mesh) return;

		const int32 SlotTotal = Mesh->GetNumMaterials();
		for (const int32 Slot : Slots)
		{
			if (Slot < 0 || Slot >= SlotTotal)
			{
				UE_LOG(LogDR, Error, TEXT("%s 머티리얼 슬롯 %d 이 범위(0~%d) 밖입니다."), *LogOwner, Slot, SlotTotal - 1);
				OutMIDs.Add(nullptr);
				continue;
			}

			UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(Slot);
			if (!MID)
			{
				UE_LOG(LogDR, Error, TEXT("%s 슬롯 %d 의 MID 생성 실패. 그 슬롯에 머티리얼이 있는지 확인하세요."), *LogOwner, Slot);
			}
			OutMIDs.Add(MID);
		}

		if (NumDigitTextures < 10)
		{
			UE_LOG(LogDR, Warning, TEXT("%s DigitTextures 가 %d개뿐입니다. 0~9 를 채우세요."), *LogOwner, NumDigitTextures);
		}
	}

	void ApplyDigits(const TArray<TObjectPtr<UMaterialInstanceDynamic>>& MIDs, FName TextureParameterName,
		const TArray<TObjectPtr<UTexture2D>>& DigitTextures, UTexture2D* EmptyTexture, const TArray<int32>& Digits)
	{
		for (int32 SlotIndex = 0; SlotIndex < MIDs.Num(); ++SlotIndex)
		{
			UMaterialInstanceDynamic* MID = MIDs[SlotIndex];
			if (!MID) continue;

			// 배열보다 자리가 많으면 남는 자리는 미표시로 둔다
			const int32 Digit = Digits.IsValidIndex(SlotIndex) ? Digits[SlotIndex] : INDEX_NONE;
			UTexture2D* Texture = DigitTextures.IsValidIndex(Digit) ? DigitTextures[Digit].Get() : nullptr;
			MID->SetTextureParameterValue(TextureParameterName, Texture ? Texture : EmptyTexture);
		}
	}
}
