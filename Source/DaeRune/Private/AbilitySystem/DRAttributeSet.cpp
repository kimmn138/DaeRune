// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/DRAttributeSet.h"
#include "Net/UnrealNetwork.h"

UDRAttributeSet::UDRAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitMana(50.f);
	InitMaxMana(50.f);
}

void UDRAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UDRAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UDRAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRAttributeSet, Health, OldHealth);
}

void UDRAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRAttributeSet, MaxHealth, OldMaxHealth);
}

void UDRAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldMana) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRAttributeSet, Mana, OldMana);
}

void UDRAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRAttributeSet, MaxMana, OldMaxMana);
}
