// Copyright DaeRune

#include "Actor/DRBellBoyHatPickup.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Character/DRCharacterBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
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
	// ★겹침 이벤트가 아니라 직접 구 쿼리★ (Plan8 T-30) — 캐릭터 캡슐은 C++ 기본값이 GenerateOverlapEvents=false 라
	// (DRCharacterBase) BP 에서 켜 둔 캐릭터(아르마딜로·플레이어)만 겹침이 잡히고 개 등은 지나가도 못 줍는다.
	// 오브젝트 타입 쿼리는 그 설정과 무관하게 Pawn 캡슐을 찾는다.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BellBoyHatPickup), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, PickupSphere->GetComponentLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(PickupSphere->GetScaledSphereRadius()), Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		TryGiveTo(Overlap.GetActor());   // 히트박스·캡슐이 같은 캐릭터로 여러 번 나와도 TryGiveTo 가 걸러낸다
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
