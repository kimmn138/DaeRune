// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2CodeScreen.generated.h"

class UMaterialInstanceDynamic;

/**
 * 숫자 표시판 (Plan6 §4.10.6 · §16.8.2)
 *
 * 퍼즐이 공개한 비밀번호 한 자리, 금고 입력 진행 상황을 표시하는 공용 표시판.
 * 자체 복제 상태를 갖지 않는다 - 상위 퍼즐/금고의 복제 상태를 받아 갱신한다.
 *
 * 표시 방식 (2026-08-27 확정):
 *   자리마다 평면 메시를 두고, 그 머티리얼의 **텍스처 파라미터를 숫자 텍스처로 교체**한다.
 *   아트 에셋의 머티리얼 인스턴스가 이미 빈 텍스처 칸을 갖고 있어 그대로 활용한다.
 *
 * BP 작업은 "평면에 태그 달고 텍스처 11장 채우기" 뿐이며 그래프를 짤 필요가 없다.
 * 커스텀 표시(TextRender 등)가 필요하면 OnDigitsChanged 훅을 추가로 구현하면 된다.
 */
UCLASS()
class DAERUNE_API ADRS2CodeScreen : public AActor
{
	GENERATED_BODY()

public:
	ADRS2CodeScreen();

	// 표시할 숫자 목록. 빈 배열이면 화면을 비운다.
	// 음수 값은 "아직 공개되지 않음"을 뜻하며 MissingTextureIndex 텍스처로 표시된다.
	UFUNCTION(BlueprintCallable, Category = "S2|Screen")
	void SetDigits(const TArray<int32>& InDigits);

	UFUNCTION(BlueprintCallable, Category = "S2|Screen")
	void ClearDigits();

	const TArray<int32>& GetDigits() const { return Digits; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Screen")
	TObjectPtr<UStaticMeshComponent> ScreenMesh;

	// ===== 자리 지정 =====

	// ★기본 방식: ScreenMesh 의 **머티리얼 슬롯** 중 숫자를 표시할 슬롯 번호들.
	//   배열 순서가 자릿수 순서(왼쪽부터)다.
	//     1자리 스크린 예: [2]        - 슬롯 2 가 숫자 표시면
	//     금고 3자리 예:   [2, 3, 4]
	//   스태틱 메시 에디터의 Material Slots 목록에서 번호를 확인한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Screen")
	TArray<int32> DigitMaterialSlots;

	// 자리 평면 머티리얼의 텍스처 파라미터 이름.
	// ★아트 에셋의 머티리얼 인스턴스에 있는 파라미터 이름과 정확히 일치해야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Screen")
	FName TextureParameterName = TEXT("BaseTexture");

	// 대안 방식: 자리마다 별도 메시 컴포넌트를 두는 경우 쓰는 태그.
	// DigitMaterialSlots 가 비어 있을 때만 사용한다.
	// 태그가 달린 컴포넌트를 이름 순으로 모아 각각 FallbackMaterialSlot 을 교체한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Screen")
	FName DigitComponentTag = TEXT("CodeDigit");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Screen", meta = (ClampMin = "0"))
	int32 FallbackMaterialSlot = 0;

	// ===== 텍스처 =====

	// 숫자 텍스처 0~9. 인덱스가 곧 숫자다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Screen")
	TArray<TObjectPtr<UTexture2D>> DigitTextures;

	// 미입력/미공개 자리에 쓸 텍스처 ("-" 또는 빈 화면).
	// 비워두면 그 자리는 텍스처를 지운다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Screen")
	TObjectPtr<UTexture2D> EmptySlotTexture;

	// 현재 표시 중인 숫자 (로컬 상태)
	UPROPERTY(BlueprintReadOnly, Category = "S2|Screen")
	TArray<int32> Digits;

	// 추가 표시 처리 (선택). C++ 텍스처 교체와 별개로 동작한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Screen")
	void OnDigitsChanged(const TArray<int32>& NewDigits);

private:
	// 자리별 MID 를 만든다 (BeginPlay 1회)
	void EnsureDigitMIDs();

	// ScreenMesh 의 머티리얼 슬롯들을 자리로 사용
	void BuildMIDsFromMaterialSlots();

	// 태그가 달린 메시 컴포넌트들을 자리로 사용
	void BuildMIDsFromTaggedComponents();

	// Digits 를 자리별 텍스처로 반영한다
	void RefreshDigitTextures();

	// 자릿값 -> 텍스처 (음수/범위 밖이면 EmptySlotTexture)
	UTexture2D* DigitToTexture(int32 Digit) const;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DigitMIDs;
};
