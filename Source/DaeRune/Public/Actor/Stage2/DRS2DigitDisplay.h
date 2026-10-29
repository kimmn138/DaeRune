// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTexture2D;

/**
 * 메시 머티리얼 슬롯에 숫자 텍스처를 띄우는 공용 처리 — 금고(ADRS2Safe)와 코드 표시판(ADRS2CodeScreen)이 같이 쓴다.
 * 슬롯마다 MID 를 하나 만들고, 자릿값(0~9)에 맞는 텍스처를 TextureParameterName 파라미터에 꽂는다.
 */
namespace DRS2DigitDisplay
{
	// Mesh 의 Slots 마다 MID 를 만든다. 범위 밖 슬롯 · 생성 실패는 null 자리로 남긴다. LogOwner 는 로그 접두어
	void BuildMIDs(UStaticMeshComponent* Mesh, const TArray<int32>& Slots, int32 NumDigitTextures,
		const FString& LogOwner, TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutMIDs);

	// MIDs[i] 에 Digits[i] 의 텍스처를 꽂는다. 음수 · 범위 밖 · 빈 텍스처 · 숫자가 모자란 자리는 EmptyTexture
	void ApplyDigits(const TArray<TObjectPtr<UMaterialInstanceDynamic>>& MIDs, FName TextureParameterName,
		const TArray<TObjectPtr<UTexture2D>>& DigitTextures, UTexture2D* EmptyTexture, const TArray<int32>& Digits);
}
