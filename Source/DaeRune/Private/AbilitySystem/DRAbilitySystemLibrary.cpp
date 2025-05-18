// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "UI/WidgetController/DRWidgetController.h"
#include "Player/DRPlayerState.h"
#include "UI/HUD/DRHUD.h"

UOverlayWidgetController* UDRAbilitySystemLibrary::GetOverlayWidgetController(const UObject* WorldContextObject)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		if (ADRHUD* DRHUD = Cast<ADRHUD>(PC->GetHUD()))
		{
			ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>();
			UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
			UAttributeSet* AS = PS->GetAttributeSet();
			const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
			return DRHUD->GetOverlayWidgetController(WidgetControllerParams);
		}
	}
	return nullptr;
}
