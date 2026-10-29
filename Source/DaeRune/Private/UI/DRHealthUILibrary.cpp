// Copyright DaeRune


#include "UI/DRHealthUILibrary.h"
#include "Blueprint/UserWidget.h"
#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "Player/DRPlayerState.h"

double UDRHealthUILibrary::ToContainerDisplayHealth(const UUserWidget* Widget, double Health, double MaxHealth)
{
	if (MaxHealth <= 0.0 || IsOwningPlayerCorrupted(Widget))
	{
		return Health;
	}

	const double DisplayMax = DisplayContainerCount * DisplayContainerSize;
	return FMath::Clamp(Health, 0.0, MaxHealth) * DisplayMax / MaxHealth;
}

bool UDRHealthUILibrary::IsOwningPlayerCorrupted(const UUserWidget* Widget)
{
	const ADRPlayerState* PS = Widget ? Widget->GetOwningPlayerState<ADRPlayerState>() : nullptr;
	if (!PS) return false;

	if (PS->HasAuthority())
	{
		if (const UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(PS->GetAttributeSet()))
		{
			return PlayerAS->IsCorrupted();
		}
	}
	return PS->IsPlayerCorrupted();
}
