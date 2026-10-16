// Copyright DaeRune

#include "Actor/DRBellBoyHatProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Actor/DRBellBoyHatPickup.h"
#include "Character/DRCharacterBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DRGameplayTags.h"
#include "Game/DRProgressionConfig.h"
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

	// ★Plan5(모자칩)★ 던진 벨보이의 칩을 1회 해석 — 판정이 서버 전용이라 서버만 (Plan5 D-5).
	// 클라의 DamageEffectParams 는 복제되지 않아 비어 있다
	if (HasAuthority())
	{
		// (지역 이름을 Tags 로 하면 AActor::Tags 를 가린다 — C4458)
		const UAbilitySystemComponent* SourceASC = DamageEffectParams.SourceAbilitySystemComponent;
		const FDRGameplayTags& DRTags = FDRGameplayTags::Get();
		bChipAllyHit = UDRAbilitySystemLibrary::HasUpgradeFeature(SourceASC, DRTags.Upgrade_Feature_Skill_Hat_AllyHit);
		ChipAllyHitDamage = UDRAbilitySystemLibrary::GetChipTuning(this).Hat.AllyHitDamage;
		ChipAllyCharges = UDRAbilitySystemLibrary::HasUpgradeFeature(SourceASC, DRTags.Upgrade_Feature_Skill_Hat_ShareDouble) ? 2 : 1;
	}
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

			// ★Plan5(모자칩)★ ① 피해 먼저, 모자는 그다음 (사용자 확정 — D-133).
			// 장착 뒤에 때리면 방금 씌운 모자가 이 피해를 막는다(T-141). 이미 쓴 모자가 있으면 그 모자가 막는다(§9-86)
			if (bChipAllyHit)
			{
				ApplyAllyHitDamage(Character);
			}

			// ★Plan5(모자칩)★ ② 칩이면 아군 직격 모자도 2회 (D-134). 실패(이미 모자 · 방금 피해로 사망)는 아래 바닥으로
			if (UDRAbilitySystemLibrary::TryEquipBellBoyHat(Character, DamageEffectParams.SourceAbilitySystemComponent, ChipAllyCharges))
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

void ADRBellBoyHatProjectile::ApplyAllyHitDamage(AActor* Ally)
{
	// 소스 ASC 가 없으면(던진 벨보이가 나감) 건너뛴다 — ApplyDamageEffect 가 역참조한다
	UAbilitySystemComponent* AllyASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Ally);
	if (!AllyASC || !DamageEffectParams.SourceAbilitySystemComponent || ChipAllyHitDamage <= 0.f) return;

	// 피해 GE · 소스 ASC · SourceAbilityTags(모자 던지기)는 그대로 — 기존 피해 경로라
	// ExecCalc 가 주는 쪽 배율(광폭화 · 시너지 · DamageDealt · 탑승①)을 곱한다 (사용자 확정 — D-133)
	FDamageEffectParams Params = DamageEffectParams;
	Params.BaseDamage = ChipAllyHitDamage;   // ★GetUpgradedDamage(적 피해 45)가 아니다 — SkillDamage 는 적 피해 축이다 (T-140)★
	Params.TargetAbilitySystemComponent = AllyASC;
	Params.KnockbackForce = FVector::ZeroVector;
	Params.DeathImpulse = GetActorForwardVector() * Params.DeathImpulseMagnitude;
	UDRAbilitySystemLibrary::ApplyDamageEffect(Params);
}

void ADRBellBoyHatProjectile::DropAsPickup(const FVector& FromLocation)
{
	// ★Plan5(가방칩 · 모자칩)★ 바닥 트레이스 · Deferred 스폰은 픽업의 공용 함수 하나로 (Plan5 D-127) — 동작은 예전과 같다.
	// 던진 본인이면 2회 (D-23), 칩 규칙(수명 · 2회)은 픽업이 던진 벨보이의 칩으로 해석한다 (Plan5 D-128)
	ADRBellBoyHatPickup::SpawnOnGround(GetWorld(), PickupClass, FromLocation, GetActorRotation().Yaw,
		GetThrower(), DamageEffectParams.SourceAbilitySystemComponent, this, PickupGroundOffset);
	Destroy();
}
