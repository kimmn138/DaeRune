// Copyright DaeRune


#include "UI/Widget/DRMinimapWidget.h"
#include "AbilitySystemComponent.h"
#include "Interaction/CombatInterface.h"
#include "DRGameplayTags.h"
#include "Character/DREnemy.h"

FVector2D UDRMinimapWidget::WorldToMinimapPosition(const FVector& WorldLocation, const FVector InPlayerLocation, float InPlayerYaw, float MinimapRange, float MinimapRadius) const
{
    // 상대 위치 (타겟 - 나)
    const FVector RelativeLocation = WorldLocation - InPlayerLocation;

    // 플레이어 Yaw 가져오기
    const float YawRad = FMath::DegreesToRadians(-InPlayerYaw);
    const float CosYaw = FMath::Cos(YawRad);
    const float SinYaw = FMath::Sin(YawRad);

    // 월드 좌표 → 플레이어 로컬 좌표로 회전
    const float LocalX = RelativeLocation.X * CosYaw - RelativeLocation.Y * SinYaw;
    const float LocalY = RelativeLocation.X * SinYaw + RelativeLocation.Y * CosYaw;

    // 플레이어 로컬 → UI 좌표
    const float Scale = MinimapRadius / MinimapRange;

    const float UI_X = LocalY * Scale;
    const float UI_Y = -LocalX * Scale;

    // Canvas 중앙 기준으로 오프셋 추가
    return FVector2D(MinimapRadius + UI_X, MinimapRadius + UI_Y);
}

bool UDRMinimapWidget::IsEnemyDetected(AActor* Enemy) const
{
    if (!Enemy)
    {
        return false;
    }

    if (ADREnemy* DREnemy = Cast<ADREnemy>(Enemy))
    {
        if (UAbilitySystemComponent* ASC = DREnemy->GetAbilitySystemComponent())
        {
            return ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Enemy_Detected);
        }
    }

    return false;
}
