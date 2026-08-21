// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRChipDragDropOperation.generated.h"

/**
 * 업그레이드 화면에서 칩을 끌 때의 페이로드. (Plan2.md 20.2 참조)
 *
 * 출발지가 두 곳이라 SourceSlotIndex 로 구분한다:
 *   -1  = 우측 칩 목록에서 시작 (장착)
 *   0.. = 좌측 슬롯에서 시작    (이동 / 해제)
 *
 * 주의: 드래그 종료 처리(하이라이트 원복)는 이 오퍼레이션의 델리게이트가 아니라
 * ★화면 위젯의 EndDragHighlight()★ 가 책임진다. 델리게이트는 위젯 파괴 순서에 따라
 * 호출되지 않을 수 있어 링이 화면에 남는다.
 */
UCLASS(BlueprintType)
class DAERUNE_API UDRChipDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Upgrade")
	FName ChipId;

	// 이 칩이 속한 로봇 (화면이 보고 있는 클래스와 다르면 드롭을 무시한다)
	UPROPERTY(BlueprintReadWrite, Category = "Upgrade")
	EPlayerCharacterClass OwnerClass = EPlayerCharacterClass::Gardener;

	// -1 = 칩 목록에서 시작. 0 이상이면 그 슬롯에서 끌어냈다는 뜻.
	UPROPERTY(BlueprintReadWrite, Category = "Upgrade")
	int32 SourceSlotIndex = -1;

	// 필요한 슬롯 수 (하이라이트/툴팁 표시용)
	UPROPERTY(BlueprintReadWrite, Category = "Upgrade")
	int32 RequiredSlotCount = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Upgrade")
	bool bHasDrawback = false;

	// 칩 목록에서 시작했는가
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	bool IsFromChipList() const { return SourceSlotIndex < 0; }
};
