// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2CodeScreen.generated.h"

/**
 * 숫자 표시판 (Plan6 §4.10.6)
 *
 * 퍼즐이 공개한 비밀번호 한 자리, 금고 입력 진행 상황을 표시하는 공용 표시판.
 * 자체 복제 상태를 갖지 않는다 - 상위 퍼즐/금고의 복제 상태를 OnRep에서 받아 갱신한다.
 * 표시 방식(머티리얼 숫자 아틀라스 / 3D 텍스트)은 BP에서 결정한다.
 */
UCLASS()
class DAERUNE_API ADRS2CodeScreen : public AActor
{
	GENERATED_BODY()

public:
	ADRS2CodeScreen();

	// 표시할 숫자 목록. 빈 배열이면 화면을 비운다.
	// 음수 값은 "아직 공개되지 않음"을 뜻하며 BP에서 '-' 등으로 표현한다.
	UFUNCTION(BlueprintCallable, Category = "S2|Screen")
	void SetDigits(const TArray<int32>& InDigits);

	UFUNCTION(BlueprintCallable, Category = "S2|Screen")
	void ClearDigits();

	const TArray<int32>& GetDigits() const { return Digits; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Screen")
	TObjectPtr<UStaticMeshComponent> ScreenMesh;

	// 현재 표시 중인 숫자 (로컬 상태)
	UPROPERTY(BlueprintReadOnly, Category = "S2|Screen")
	TArray<int32> Digits;

	// 실제 표시 갱신 (머티리얼 파라미터 세팅 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Screen")
	void OnDigitsChanged(const TArray<int32>& NewDigits);
};
