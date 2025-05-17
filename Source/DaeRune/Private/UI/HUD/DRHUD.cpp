// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/DRHUD.h"
#include "UI/Widget/DRUserWidget.h"

void ADRHUD::BeginPlay()
{
	Super::BeginPlay();

	UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), OverlayWidgetClass);
	Widget->AddToViewport();
}
