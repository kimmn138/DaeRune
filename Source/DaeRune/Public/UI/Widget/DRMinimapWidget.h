// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "DRMinimapWidget.generated.h"

// 미니맵 아이콘 타입
UENUM(BlueprintType)
enum class EMinimapIconType : uint8
{
    Player,
    Ally,
    Enemy,
    Objective
};

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRMinimapWidget : public UDRUserWidget
{
	GENERATED_BODY()
	
public:
    // 월드 좌표를 미니맵 좌표로 변환
    UFUNCTION(BlueprintCallable, Category = "Minimap")
    FVector2D WorldToMinimapPosition(const FVector& WorldLocation, const FVector InPlayerLocation, float InPlayerYaw, float MinimapRange, float MinimapRadius) const;

    // 적이 발각 상태인지 확인
    UFUNCTION(BlueprintCallable, Category = "Minimap")
    bool IsEnemyDetected(AActor* Enemy) const;
};
