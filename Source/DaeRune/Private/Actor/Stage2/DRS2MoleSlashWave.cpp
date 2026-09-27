// Copyright DaeRune


#include "Actor/Stage2/DRS2MoleSlashWave.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Character/DRCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "DaeRune/DaeRune.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"

ADRS2MoleSlashWave::ADRS2MoleSlashWave()
{
	// ★부모는 Tick 을 끈다. 지형 판정(수평 선분 트레이스)을 위해 다시 켠다.
	PrimaryActorTick.bCanEverTick = true;

	// ---- 대각 칼날 판정 캡슐 ----
	// 축(로컬 Z)을 액터 Y 로 눕힌다. 기울기(±45)는 액터 스폰 회전의 Roll 이 준다.
	BladeCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BladeCapsule"));
	BladeCapsule->SetupAttachment(GetRootComponent());
	BladeCapsule->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	BladeCapsule->SetCapsuleSize(50.f, 200.f);

	BladeCapsule->SetCollisionObjectType(ECC_Projectile);
	BladeCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BladeCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	BladeCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BladeCapsule->OnComponentBeginOverlap.AddDynamic(this, &ADRS2MoleSlashWave::OnSphereOverlap);
}

void ADRS2MoleSlashWave::BeginPlay()
{
	Super::BeginPlay();

	// ★루트 스피어의 판정을 완전히 끈다 — 판정 형태를 기울어진 캡슐 하나로 단일화한다.
	//  ① 스피어가 살아 있으면 대각선의 "빈 구석"이 중앙 구로 메워져 회피 설계가 무의미해진다
	//  ② 지면 위 100uu 를 나는 물체라 스피어가 바닥(WorldStatic)과 상시 오버랩된다
	//  (부모 BeginPlay 가 Sphere 에 건 델리게이트는 콜백이 오지 않으므로 그대로 둬도 무해하다)
	if (Sphere)
	{
		Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// ★부모 BeginPlay 가 자기 LifeSpan(기본 15초)으로 덮어쓴 뒤이므로 여기서 다시 적용한다.
	if (PendingLifeSpan > 0.f)
	{
		SetLifeSpan(PendingLifeSpan);
	}

	LastTickLocation = GetActorLocation();
}

void ADRS2MoleSlashWave::InitWave(float InSpeed, float InMaxRange)
{
	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = InSpeed;
		ProjectileMovement->MaxSpeed = InSpeed;
		ProjectileMovement->ProjectileGravityScale = 0.f;   // 고도 고정

		// ★대각 기울기(Roll)가 매 프레임 속도 방향으로 덮어써지는 것을 막는다.
		//   이게 true 면 칼날이 항상 수평으로 돌아와 대각선 사양이 통째로 사라진다.
		ProjectileMovement->bRotationFollowsVelocity = false;
	}

	// 수명 = 사거리 / 속도. 실제 적용은 BeginPlay (부모가 덮어쓰기 때문).
	PendingLifeSpan = (InSpeed > KINDA_SMALL_NUMBER) ? (InMaxRange / InSpeed) : 1.f;
	SetLifeSpan(PendingLifeSpan);
}

void ADRS2MoleSlashWave::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || bHit || !bStopOnWorldGeometry)
	{
		LastTickLocation = GetActorLocation();
		return;
	}

	// ★지형 판정을 오버랩이 아니라 프레임 간 선분 트레이스로 하는 이유:
	//   검기는 고도가 고정이라 이 선분이 **항상 수평**이다. 그래서 바닥과는 절대 교차하지 않고
	//   수직 지오메트리(벽·열차)만 잡힌다. 오버랩으로 WorldStatic 을 보면 바닥에 걸려 즉시 소멸한다.
	//   빠른 이동에서도 프레임 사이를 선분이 이어 주므로 터널링이 없다.
	const FVector Now = GetActorLocation();

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (DamageEffectParams.SourceAbilitySystemComponent)
	{
		Params.AddIgnoredActor(DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor());
	}

	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, LastTickLocation, Now, ECC_Visibility, Params))
	{
		SetActorLocation(Hit.ImpactPoint);
		OnHit();            // 임팩트 연출 + bHit = true
		Destroy();
		return;
	}

	LastTickLocation = Now;
}

void ADRS2MoleSlashWave::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (DamageEffectParams.SourceAbilitySystemComponent == nullptr) return;

	AActor* SourceAvatarActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();
	if (!IsValid(SourceAvatarActor) || !IsValid(OtherActor)) return;
	if (SourceAvatarActor == OtherActor) return;
	if (!UDRAbilitySystemLibrary::IsNotFriend(SourceAvatarActor, OtherActor)) return;

	// ★플레이어만 때린다. 벽·소품이 여기 들어와도 걸러진다 (지형 정지는 Tick 담당).
	//   "Player" 액터 태그가 아니라 타입으로 판별한다 — 태그는 BP 설정에만 있어 레벨 실수에 취약하다.
	if (Cast<ADRCharacter>(OtherActor) == nullptr) return;

	// 동일 대상 재타격 금지 (관통 중 오버랩 재진입 방지)
	if (HitActors.Contains(OtherActor)) return;
	HitActors.Add(OtherActor);

	OnPierceVisual(OtherActor->GetActorLocation());

	if (!HasAuthority()) return;

	if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
	{
		DamageEffectParams.DeathImpulse = GetActorForwardVector() * DamageEffectParams.DeathImpulseMagnitude;

		const bool bKnockback = FMath::RandRange(1, 100) < DamageEffectParams.KnockbackChance;
		if (bKnockback)
		{
			FRotator Rotation = GetActorRotation();
			Rotation.Pitch = 45.f;
			// ★Roll 은 칼날 기울기라 넉백 방향에 섞이면 안 된다.
			Rotation.Roll = 0.f;

			DamageEffectParams.KnockbackForce = Rotation.Vector() * DamageEffectParams.KnockbackForceMagnitude;
		}

		DamageEffectParams.TargetAbilitySystemComponent = TargetASC;
		UDRAbilitySystemLibrary::ApplyDamageEffect(DamageEffectParams);
	}

	// ★MaxPierceCount = 0 이면 이 블록에 절대 들어가지 않는다 = 무제한 관통.
	//   검기를 멈추는 것은 오직 지형(Tick)과 수명뿐이다.
	if (MaxPierceCount > 0 && ++PierceCount >= MaxPierceCount)
	{
		OnHit();
		Destroy();
	}
}
