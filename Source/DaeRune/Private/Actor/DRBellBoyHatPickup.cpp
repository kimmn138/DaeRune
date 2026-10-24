// Copyright DaeRune

#include "Actor/DRBellBoyHatPickup.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Character/DRCharacterBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DRGameplayTags.h"
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

ADRBellBoyHatPickup* ADRBellBoyHatPickup::SpawnOnGround(UWorld* World, TSubclassOf<ADRBellBoyHatPickup> PickupClass,
	const FVector& FromLocation, float Yaw, AActor* InThrower, UAbilitySystemComponent* InThrowerASC,
	const AActor* IgnoreActor, float GroundOffset)
{
	// 서버 전용 — 픽업은 서버가 스폰해 복제한다. 클라에서 부르면 아무것도 안 한다
	if (!World || !PickupClass || World->GetNetMode() == NM_Client) return nullptr;

	// 바닥 찾기 — ★오브젝트 타입(WorldStatic/Dynamic)으로★ 쏜다.
	// Visibility 채널로 쏘면 적 히트박스(Visibility Block)에 맞아 픽업이 공중에 뜬다 (Plan8 T-17)
	FVector SpawnLocation = FromLocation;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BellBoyHatDrop), false, IgnoreActor);

	FHitResult GroundHit;
	if (World->LineTraceSingleByObjectType(GroundHit, FromLocation + FVector(0.f, 0.f, 50.f),
			FromLocation - FVector(0.f, 0.f, 5000.f), ObjectParams, QueryParams))
	{
		SpawnLocation = GroundHit.ImpactPoint + FVector(0.f, 0.f, GroundOffset);
	}

	const FTransform SpawnTransform(FRotator(0.f, Yaw, 0.f), SpawnLocation);
	ADRBellBoyHatPickup* Pickup = World->SpawnActorDeferred<ADRBellBoyHatPickup>(
		PickupClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup) return nullptr;

	// ★FinishSpawning 전★ — BeginPlay 의 SetLifeSpan 이 칩③ 수명을 읽어야 한다 (Plan5 T-142)
	Pickup->InitThrower(InThrower, InThrowerASC);   // 던진 벨보이 본인이면 2회 (D-23)
	Pickup->FinishSpawning(SpawnTransform);
	return Pickup;
}

void ADRBellBoyHatPickup::InitThrower(AActor* InThrower, UAbilitySystemComponent* InThrowerASC)
{
	Thrower = InThrower;
	ThrowerASC = InThrowerASC;

	// ★Plan5(모자칩)★ 바닥 모자 규칙은 "던진 벨보이의 모자칩"을 따른다 — 가방칩②가 떨군 모자도 같다 (Plan5 D-128).
	// 키는 모자 던지기 GA 태그로 고정한다. 던진 사람의 ASC 가 없으면(나감) 빈 캐시라 10초 · 본인 2회 그대로다
	// (지역 이름을 Tags 로 하면 AActor::Tags 를 가린다 — C4458)
	const FDRGameplayTags& DRTags = FDRGameplayTags::Get();
	const FDRUpgradeRuntime& Runtime = UDRAbilitySystemLibrary::GetUpgradeRuntimeForASC(InThrowerASC);
	EffectiveLifeTime = FMath::Max(0.1f,
		Runtime.ApplySkill(DRTags.Abilities_BellBoy_HatThrow, EDRUpgradeStat::SkillDuration, LifeTime));   // ③ 10 → 18초
	bShareDoubleCharges = Runtime.HasFeature(DRTags.Upgrade_Feature_Skill_Hat_ShareDouble);              // ② 플레이어 전원 2회
}

void ADRBellBoyHatPickup::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) return;

	// 10초(모자칩③ 18초) 뒤 서버가 Destroy → 복제. InitThrower 없이 스폰된 픽업은 LifeTime 그대로
	SetLifeSpan(EffectiveLifeTime > 0.f ? EffectiveLifeTime : LifeTime);
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
	// ★Plan5(모자칩)★ ② 던진 벨보이가 칩②면 플레이어 전원 2회 — 적은 그대로 1회 (Plan5 D-134 · T-148)
	const bool bIsThrower = Thrower.IsValid() && Character == Thrower.Get();
	const bool bSharedPlayer = bShareDoubleCharges && Character->ActorHasTag(FName("Player"));
	const int32 Charges = (bIsThrower || bSharedPlayer) ? ThrowerCharges : 1;
	if (UDRAbilitySystemLibrary::TryEquipBellBoyHat(Character, ThrowerASC.Get(), Charges))
	{
		bConsumed = true;
		Destroy();
	}
	// 실패(이미 모자를 쓰고 있음) → 픽업은 그대로 남는다 (D-22)
}
