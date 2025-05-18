// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/DRCharacterBase.h"

ADRCharacterBase::ADRCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	Weapon = CreateDefaultSubobject<USkeletalMeshComponent>("Weapon");
	Weapon->SetupAttachment(GetMesh(), FName("WeaponHandSocket"));
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

UAbilitySystemComponent* ADRCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADRCharacterBase::BeginPlay()
{
	Super::BeginPlay();
}

void ADRCharacterBase::InitAbilityActorInfo()
{
}



