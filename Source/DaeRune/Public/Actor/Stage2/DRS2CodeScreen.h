// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2CodeScreen.generated.h"

class UMaterialInstanceDynamic;

/**
 * 숫자 표시판 (Plan6 §4.10.6 · §16.8.2)
 *
 * 8퍼즐 / 스위치 퍼즐이 공개한 비밀번호 한 자리를 표시한다.
 * 자체 복제 상태를 갖지 않는다 - 상위 퍼즐의 복제 상태를 받아 갱신한다.
 *
 * 표시 방식은 금고(ADRS2Safe)와 **동일**하다 (2026-08-27 통일):
 *   ScreenMesh 의 머티리얼 슬롯마다 MID 를 만들고, 그 텍스처 파라미터를 숫자 텍스처로 교체한다.
 *   프로퍼티 이름과 동작이 금고와 같으므로 한쪽을 익히면 양쪽 다 같은 방식으로 설정한다.
 *
 * BP 그래프를 짤 필요가 없다. 추가 연출이 필요하면 OnDigitsChanged 훅을 구현한다.
 */
UCLASS()
class DAERUNE_API ADRS2CodeScreen : public AActor
{
	GENERATED_BODY()

public:
	ADRS2CodeScreen();

	// 표시할 숫자 목록. 빈 배열이면 전 자리를 미표시로 둔다.
	// 음수 값은 "아직 공개되지 않음"을 뜻하며 EmptySlotTexture 로 표시된다.
	UFUNCTION(BlueprintCallable, Category = "S2|Screen")
	void SetDigits(const TArray<int32>& InDigits);

	UFUNCTION(BlueprintCallable, Category = "S2|Screen")
	void ClearDigits();

	const TArray<int32>& GetDigits() const { return Digits; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Screen")
	TObjectPtr<UStaticMeshComponent> ScreenMesh;

	// ===== 표시 설정 (금고 §16.8.6-B 와 동일한 항목들) =====

	// 숫자를 표시할 머티리얼 슬롯 번호들. 배열 순서 = 자릿수 순서(왼쪽부터).
	// 퍼즐 스크린은 1자리이므로 보통 항목 1개다. 예: [2]
	// 스태틱 메시 에디터의 Material Slots 목록에서 번호를 확인한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Screen|표시")
	TArray<int32> DigitMaterialSlots;

	// 머티리얼의 텍스처 파라미터 이름.
	// ★에셋 머티리얼 인스턴스의 파라미터 이름과 정확히 일치해야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Screen|표시")
	FName TextureParameterName = TEXT("BaseTexture");

	// 숫자 텍스처 0~9. ★인덱스가 곧 숫자다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Screen|표시")
	TArray<TObjectPtr<UTexture2D>> DigitTextures;

	// 미공개 자리에 쓸 텍스처 ("-" 또는 빈 화면). 비우면 텍스처를 지운다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Screen|표시")
	TObjectPtr<UTexture2D> EmptySlotTexture;

	// 현재 표시 중인 숫자 (로컬 상태)
	UPROPERTY(BlueprintReadOnly, Category = "S2|Screen")
	TArray<int32> Digits;

	// 추가 연출 (선택). C++ 텍스처 교체와 별개로 발화한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Screen")
	void OnDigitsChanged(const TArray<int32>& NewDigits);

private:
	// 자리 슬롯마다 MID 를 만든다 (BeginPlay 1회)
	void EnsureDisplayMIDs();

	// Digits 를 자리별 텍스처로 반영한다
	void ApplyDigitTextures();

	// 자릿값 -> 텍스처 (음수/범위 밖이면 EmptySlotTexture)
	UTexture2D* DigitToTexture(int32 Digit) const;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DisplayMIDs;
};
