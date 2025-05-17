// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/DREffectActor.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Components/SphereComponent.h"

ADREffectActor::ADREffectActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	SetRootComponent(Mesh);

	Sphere = CreateDefaultSubobject<USphereComponent>("Sphere");
	Sphere->SetupAttachment(GetRootComponent());
}

void ADREffectActor::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	//TODO: Change this to apply a Gameplay Effect. For now, using const_cast as a hack!
	if (IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(OtherActor))
	{
		const UDRAttributeSet* DRAttributeSet = Cast<UDRAttributeSet>(ASCInterface->GetAbilitySystemComponent()->GetAttributeSet(UDRAttributeSet::StaticClass()));

		UDRAttributeSet* MutableDRAttributeSet = const_cast<UDRAttributeSet*>(DRAttributeSet);
		MutableDRAttributeSet->SetHealth(DRAttributeSet->GetHealth() + 25.f);
		Destroy();
	}
}

void ADREffectActor::EndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
}

void ADREffectActor::BeginPlay()
{
	Super::BeginPlay();

	Sphere->OnComponentBeginOverlap.AddDynamic(this, &ADREffectActor::OnOverlap);
	Sphere->OnComponentEndOverlap.AddDynamic(this, &ADREffectActor::EndOverlap);
}
