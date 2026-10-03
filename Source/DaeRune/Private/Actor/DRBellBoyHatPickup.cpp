// Copyright DaeRune

#include "Actor/DRBellBoyHatPickup.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Character/DRCharacterBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/CombatInterface.h"
#include "TimerManager.h"

ADRBellBoyHatPickup::ADRBellBoyHatPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;   // 서버가 스폰, 모든 클라가 본다 (움직이지 않으므로 이동 복제는 끈 채로)

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	SetRootComponent(PickupSphere);
	PickupSphere->InitSphereRadius(60.f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionObjectType(ECC_WorldDynamic);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);   // 캐릭터 캡슐만
	PickupSphere->SetGenerateOverlapEvents(true);

	HatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HatMesh"));
	HatMesh->SetupAttachment(PickupSphere);
	HatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRBellBoyHatPickup::InitThrower(AActor* InThrower, UAbilitySystemComponent* InThrowerASC)
{
	Thrower = InThrower;
	ThrowerASC = InThrowerASC;
}

void ADRBellBoyHatPickup::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) return;

	SetLifeSpan(LifeTime);   // 10초 뒤 서버가 Destroy → 복제
	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &ADRBellBoyHatPickup::OnPickupOverlap);

	// ★즉시 1회 + 주기 재확인★ — BeginOverlap 만으로는 "이미 올라서 있던 대상"이 모자를 잃은 뒤 못 줍는다 (D-25)
	GetWorldTimerManager().SetTimer(RecheckTimerHandle, this, &ADRBellBoyHatPickup::RecheckOverlaps,
		RecheckInterval, true, 0.f);
}

void ADRBellBoyHatPickup::OnPickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryGiveTo(OtherActor);
}

void ADRBellBoyHatPickup::RecheckOverlaps()
{
	TArray<AActor*> Overlapping;
	PickupSphere->GetOverlappingActors(Overlapping, ADRCharacterBase::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		TryGiveTo(Actor);
		if (bConsumed) return;
	}
}

void ADRBellBoyHatPickup::TryGiveTo(AActor* Candidate)
{
	if (bConsumed || !HasAuthority()) return;

	ADRCharacterBase* Character = Cast<ADRCharacterBase>(Candidate);
	if (!IsValid(Character) || ICombatInterface::Execute_IsDead(Character)) return;

	// 적이든 아군이든 (사용자 확정). 던진 벨보이 본인이면 2회 (D-23)
	const int32 Charges = (Thrower.IsValid() && Character == Thrower.Get()) ? ThrowerCharges : 1;
	if (UDRAbilitySystemLibrary::TryEquipBellBoyHat(Character, ThrowerASC.Get(), Charges))
	{
		bConsumed = true;
		Destroy();
	}
	// 실패(이미 모자를 쓰고 있음) → 픽업은 그대로 남는다 (D-22)
}
