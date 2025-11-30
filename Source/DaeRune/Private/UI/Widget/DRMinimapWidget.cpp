// Copyright DaeRune


#include "UI/Widget/DRMinimapWidget.h"
#include "AbilitySystemComponent.h"
#include "Interaction/CombatInterface.h"
#include "DRGameplayTags.h"
#include "Character/DREnemy.h"

FVector2D UDRMinimapWidget::WorldToMinimapPosition(const FVector& WorldLocation, float MinimapRange, float MinimapRadius) const
{
    APawn* LocalPawn = GetOwningPlayerPawn();
    if (!LocalPawn || MinimapRange <= 0.f)
    {
        return FVector2D::ZeroVector;
    }

    const FVector PlayerLocation = LocalPawn->GetActorLocation();
    const FVector RelativeLocation = WorldLocation - PlayerLocation;

    const float PlayerYaw = LocalPawn->GetControlRotation().Yaw;
    const float YawRad = FMath::DegreesToRadians(PlayerYaw);
    const float CosYaw = FMath::Cos(YawRad);
    const float SinYaw = FMath::Sin(YawRad);

    const float RotatedX = RelativeLocation.X * CosYaw + RelativeLocation.Y * SinYaw;
    const float RotatedY = -RelativeLocation.X * SinYaw + RelativeLocation.Y * CosYaw;

    const float Scale = MinimapRadius / MinimapRange;

    return FVector2D(RotatedY * Scale, -RotatedX * Scale);
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
