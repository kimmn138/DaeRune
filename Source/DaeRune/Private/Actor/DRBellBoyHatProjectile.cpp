// Copyright DaeRune

#include "Actor/DRBellBoyHatProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Actor/DRBellBoyHatPickup.h"
#include "Character/DRCharacterBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Interaction/CombatInterface.h"
#include "Interaction/DRProximityHitOnly.h"

ADRBellBoyHatProjectile::ADRBellBoyHatProjectile()
{
	HatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HatMesh"));
	HatMesh->SetupAttachment(GetRootComponent());
	HatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SpinComponent = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("Spin"));
	SpinComponent->RotationRate = FRotator(0.f, 720.f, 0.f);   // Yaw 로 빙그르르
	SpinComponent->bAutoRegisterUpdatedComponent = false;     // ★루트(구)가 아니라 모자 메시만 돈다 → BeginPlay★

	// 부모 기본값(550, 중력 0)을 모자에 맞게 — BP 에서 다시 조정 가능
	ProjectileMovement->InitialSpeed = 1400.f;
	ProjectileMovement->MaxSpeed = 1400.f;
	ProjectileMovement->ProjectileGravityScale = 0.35f;       // 아무도 안 맞으면 바닥으로 떨어진다
}

void ADRBellBoyHatProjectile::BeginPlay()
{
	Super::BeginPlay();
	SpinComponent->SetUpdatedComponent(HatMesh);   // 연출이라 모든 머신에서. 루트를 돌리면 비행 방향이 틀어진다
}

AActor* ADRBellBoyHatProjectile::GetThrower() const
{
	// 스폰할 때 Instigator 는 비어 있다(플레이어 ASC 소유자가 PlayerState) — 소스 ASC 의 아바타가 던진 벨보이다
	return DamageEffectParams.SourceAbilitySystemComponent
		? DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor() : nullptr;
}

void ADRBellBoyHatProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 판정은 서버만. 클라는 서버의 Destroy 복제를 기다린다 (부모 Destroyed 가 클라 임팩트 연출 — T-21)
	if (!HasAuthority() || bResolved) return;

	AActor* Thrower = GetThrower();
	if (!IsValid(OtherActor) || OtherActor == this || OtherActor == Thrower) return;   // 스폰 위치가 캡슐 안 (T-15)
	if (OtherActor->IsA<ADRProjectile>() || OtherActor->IsA<ADRBellBoyHatPickup>()) return;

	// ① 캐릭터 — 아군 / 적
	if (ADRCharacterBase* Character = Cast<ADRCharacterBase>(OtherActor))
	{
		if (ICombatInterface::Execute_IsDead(Character)) return;   // 시체는 통과

		if (!UDRAbilitySystemLibrary::IsNotFriend(Thrower, Character))   // 아군 = 둘 다 "Player" 태그
		{
			bResolved = true;
			if (UDRAbilitySystemLibrary::TryEquipBellBoyHat(Character, DamageEffectParams.SourceAbilitySystemComponent, 1))
			{
				Destroy();                              // 머리에 씌웠다
			}
			else
			{
				DropAsPickup(GetActorLocation());       // ★이미 모자 → 부딪혀 그 자리 바닥에 (D-22, 사용자 확정)★
			}
			return;
		}

		HandleEnemyHit(Character);                  // 적 → 피해 30 후 소멸
		return;
	}

	// ② 캐릭터가 아닌 전투 대상(홀로그램 두더지) — 적과 같게 (근접 규칙은 ApplyDamageEffect 가 처리)
	if (Cast<IDRProximityHitOnly>(OtherActor))
	{
		HandleEnemyHit(OtherActor);
		return;
	}

	// ③ 벽·바닥 = 폰을 막는 표면만 (D-24) — 트리거 볼륨(Overlap 전용)은 무시. 클렌저 사이트도 여기로 온다
	if (OtherComp && OtherComp->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
	{
		bResolved = true;
		DropAsPickup(GetActorLocation());
	}
}

void ADRBellBoyHatProjectile::HandleEnemyHit(AActor* Target)
{
	bResolved = true;
	if (!bHit) OnHit();   // 부모의 임팩트 이펙트·사운드

	if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
	{
		DamageEffectParams.TargetAbilitySystemComponent = TargetASC;
		DamageEffectParams.DeathImpulse = GetActorForwardVector() * DamageEffectParams.DeathImpulseMagnitude;
		UDRAbilitySystemLibrary::ApplyDamageEffect(DamageEffectParams);   // BaseDamage = GA 의 Damage(30)
	}
	Destroy();
}

void ADRBellBoyHatProjectile::DropAsPickup(const FVector& FromLocation)
{
	if (PickupClass)
	{
		// 바닥 찾기 — ★오브젝트 타입(WorldStatic/Dynamic)으로★ 쏜다.
		// Visibility 채널로 쏘면 적 히트박스(Visibility Block)에 맞아 픽업이 공중에 뜬다 (T-17)
		FVector SpawnLocation = FromLocation;
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BellBoyHatDrop), false, this);

		FHitResult GroundHit;
		if (GetWorld()->LineTraceSingleByObjectType(GroundHit, FromLocation + FVector(0.f, 0.f, 50.f),
				FromLocation - FVector(0.f, 0.f, 5000.f), ObjectParams, QueryParams))
		{
			SpawnLocation = GroundHit.ImpactPoint + FVector(0.f, 0.f, PickupGroundOffset);
		}

		const FTransform SpawnTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), SpawnLocation);
		if (ADRBellBoyHatPickup* Pickup = GetWorld()->SpawnActorDeferred<ADRBellBoyHatPickup>(
				PickupClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Pickup->InitThrower(GetThrower(), DamageEffectParams.SourceAbilitySystemComponent);   // 던진 본인이면 2회 (D-23)
			Pickup->FinishSpawning(SpawnTransform);
		}
	}
	Destroy();
}
