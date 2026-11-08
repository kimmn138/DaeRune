// Copyright DaeRune


#include "Actor/DRVacuumAirProjectile.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Character/DRCharacter.h"
#include "Character/DRRobotVacuumCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "DaeRune/DaeRune.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

ADRVacuumAirProjectile::ADRVacuumAirProjectile()
{
	// 가로형 판정 박스 (Plan5 §7) — 루트 Sphere는 비균등 스케일이 불가능하므로 박스로 판정 이관.
	// Extent · 위치는 BP_VacuumAirProjectile 에서 날 이펙트(WindBlade_NS)에 맞춘다.
	// 예전엔 여기서 NoCollision 으로 두고 "BP 에서 켠다"였는데 BP 가 켜지 않아 박스가 꺼진 채였다 → 판정이 반지름 25 구뿐이었다.
	// ★폰에만 오버랩★ — 벽 · 바닥은 루트 Sphere 가 맡는다. 박스가 벽에도 닿으면 벽 옆에서 쏜 탄이 날 끝에 걸려 바로 사라진다.
	// 오브젝트 타입은 Sphere 와 같은 Projectile — 커스텀 히트박스 적은 캡슐이 Projectile 을 무시하고 히트박스만 응답한다 (ADREnemy)
	WideCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("WideCollision"));
	WideCollision->SetupAttachment(GetRootComponent());   // 루트 = Sphere
	WideCollision->SetCollisionObjectType(ECC_Projectile);
	WideCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WideCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	WideCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	WideCollision->SetGenerateOverlapEvents(true);

	// 바닥 활주 — 벽 · 바닥(WorldStatic)과는 오버랩 대신 블록으로 부딪혀 ProjectileMovement 가 반발 이벤트를 낸다.
	// 반발 0 · 마찰 0 이라 바닥에 닿으면 수직 성분만 사라지고 바닥을 따라간다. 벽 · 천장은 HandleProjectileBounce 가 적중 처리한다.
	// (Stage1/2 에서 Projectile 채널을 막는 WorldStatic 은 BlockingVolume · 자동문뿐 — 메시는 원래 Projectile 을 무시한다)
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.f;
	ProjectileMovement->Friction = 0.f;
	ProjectileMovement->bRotationFollowsVelocity = true;   // 미끄러지는 동안 날이 바닥과 나란히 눕는다
}

void ADRVacuumAirProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRVacuumAirProjectile, bEnhanced);
	DOREPLIFETIME(ADRVacuumAirProjectile, bFading);
	DOREPLIFETIME(ADRVacuumAirProjectile, bHitsAllies);   // ★Plan5(공기탄칩)★
	DOREPLIFETIME(ADRVacuumAirProjectile, bSkimming);
}

void ADRVacuumAirProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 부모의 LifeSpan 자동 소멸을 취소하고 자체 페이드 타이머 사용 (Plan3 §6.2)
	SetLifeSpan(0.f);

	if (HasAuthority())
	{
		LiftAboveFloor();
	}

	ProjectileMovement->OnProjectileBounce.AddUniqueDynamic(this, &ADRVacuumAirProjectile::HandleProjectileBounce);

	// 바닥 먼지는 보이는 머신에서만 (데디서버 제외)
	if (GetNetMode() != NM_DedicatedServer && SkimTrailEffect)
	{
		GetWorldTimerManager().SetTimer(SkimTrailTimerHandle, this, &ADRVacuumAirProjectile::SpawnSkimTrail, SkimTrailInterval, true);
	}

	// 이 직후 엔진이 스폰 순간 겹친 대상의 오버랩을 처리한다 (같은 프레임) — OnSphereOverlap 의 아군 예외용
	SpawnFrame = GFrameCounter;

	// 가로형 판정 박스 오버랩 — 부모(Sphere)처럼 BeginPlay 에서 묶는다.
	// 예전 생성자 바인딩이 BP_VacuumAirProjectile 에 직렬화돼 남아 있어 AddUnique 로 이중 바인딩을 막는다.
	WideCollision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ADRVacuumAirProjectile::OnSphereOverlap);

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(ActiveTimerHandle, this, &ADRVacuumAirProjectile::StartFade, ActiveDuration, false);
	}
}

void ADRVacuumAirProjectile::Destroyed()
{
	// 페이드 소멸은 임팩트 연출(사운드/나이아가라) 생략 — 부모 Destroyed의 OnHit 호출 차단
	if (bFading) bHit = true;
	Super::Destroyed();
}

void ADRVacuumAirProjectile::StartFade()
{
	if (bFading || bHit) return;
	bFading = true;

	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (WideCollision) WideCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMovement->StopMovementImmediately();

	// 리슨서버 로컬 연출 (클라는 RepNotify 경로)
	OnRep_Fading();

	GetWorldTimerManager().SetTimer(FadeTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		Destroy();
	}), FadeDuration, false);
}

void ADRVacuumAirProjectile::OnRep_Fading()
{
	if (bFading)
	{
		OnFadeStarted();
	}
}

// ========== 바닥 활주 ==========

void ADRVacuumAirProjectile::LiftAboveFloor()
{
	// 청소기 총구는 아래를 볼수록 바닥에 붙는다(−20° 에 바닥 위 25, −30° 에 19 — 구 반지름 25).
	// 바닥에 박힌 채 시작하면 첫 이동이 바닥 밀어내기로 끝나므로 구 아래 끝이 바닥에 닿는 높이까지 올린다 (최대 ~6uu)
	const float Radius = Sphere->GetScaledSphereRadius();
	const FVector Location = GetActorLocation();
	FHitResult Floor;
	if (FindFloorBelow(Location + FVector(0.f, 0.f, Radius), Radius * 2.f + 1.f, Floor))
	{
		const float MinZ = Floor.ImpactPoint.Z + Radius + 1.f;
		if (Location.Z < MinZ)
		{
			SetActorLocation(FVector(Location.X, Location.Y, MinZ), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

bool ADRVacuumAirProjectile::FindFloorBelow(const FVector& From, float Distance, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VacuumAirFloor), false, this);
	FCollisionResponseParams ResponseParams(Sphere->GetCollisionResponseToChannels());
	return GetWorld()->LineTraceSingleByChannel(OutHit, From, From - FVector(0.f, 0.f, Distance),
			Sphere->GetCollisionObjectType(), Params, ResponseParams)
		&& !OutHit.bStartPenetrating
		&& OutHit.ImpactNormal.Z >= FloorNormalMinZ;
}

void ADRVacuumAirProjectile::HandleProjectileBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	if (bFading) return;

	// 바닥 — 반발 0 · 마찰 0 으로 이미 바닥과 나란해진 속도를 원래 빠르기로 되돌린다 (비스듬히 닿을수록 cos 만큼 느려지므로)
	if (ImpactResult.ImpactNormal.Z >= FloorNormalMinZ)
	{
		FVector SlideDir = FVector::VectorPlaneProject(ImpactVelocity, ImpactResult.ImpactNormal).GetSafeNormal();
		if (SlideDir.IsNearlyZero())
		{
			SlideDir = FVector::VectorPlaneProject(GetActorForwardVector(), ImpactResult.ImpactNormal).GetSafeNormal();
		}
		ProjectileMovement->Velocity = SlideDir * ImpactVelocity.Size();

		PlaySkimTouchdown();
		if (HasAuthority() && !bSkimming)
		{
			bSkimming = true;
		}
		return;
	}

	// 벽 · 천장 — 예전 WorldStatic 오버랩과 같은 적중 처리 (서버: 임팩트 · 피해 대상이면 피해 · 파괴)
	if (HasAuthority())
	{
		OnSphereOverlap(Sphere, ImpactResult.GetActor(), ImpactResult.GetComponent(), ImpactResult.Item, true, ImpactResult);
	}
	else
	{
		// 클라: 벽을 따라 미끄러지지 않고 멈춰 서버의 파괴를 기다린다 (Destroyed 에서 임팩트 연출)
		ProjectileMovement->Velocity = FVector::ZeroVector;
	}
}

void ADRVacuumAirProjectile::OnRep_Skimming()
{
	if (bSkimming)
	{
		PlaySkimTouchdown();
	}
}

void ADRVacuumAirProjectile::PlaySkimTouchdown()
{
	if (bTouchdownPlayed || GetNetMode() == NM_DedicatedServer || !SkimTouchdownEffect) return;
	bTouchdownPlayed = true;

	// 고리가 바닥에 눕도록 바닥 법선을 위로 (못 찾으면 구 아래 끝)
	const float Radius = Sphere->GetScaledSphereRadius();
	FHitResult Floor;
	const bool bFoundFloor = FindFloorBelow(GetActorLocation(), Radius + SkimTrailMaxGap + 10.f, Floor);
	const FVector Location = bFoundFloor ? Floor.ImpactPoint : GetActorLocation() - FVector(0.f, 0.f, Radius);
	const FVector Up = bFoundFloor ? FVector(Floor.ImpactNormal) : FVector::UpVector;
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, SkimTouchdownEffect, Location,
		FRotationMatrix::MakeFromZX(Up, GetActorForwardVector()).Rotator());
}

void ADRVacuumAirProjectile::SpawnSkimTrail()
{
	if (bFading || !SkimTrailEffect) return;

	FHitResult Floor;
	if (!FindFloorBelow(GetActorLocation(), Sphere->GetScaledSphereRadius() + SkimTrailMaxGap, Floor)) return;

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, SkimTrailEffect, Floor.ImpactPoint,
		FRotationMatrix::MakeFromZX(Floor.ImpactNormal, GetActorForwardVector()).Rotator(),
		FVector(1.f), true, true, ENCPoolMethod::AutoRelease);
}

void ADRVacuumAirProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bFading) return;
	if (DamageEffectParams.SourceAbilitySystemComponent == nullptr) return;
	AActor* SourceAvatarActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();
	if (!IsValid(SourceAvatarActor) || !IsValid(OtherActor)) return;
	if (SourceAvatarActor == OtherActor) return;

	// 시전자와 탑승 링크로 붙어있는 캐릭터는 통과 (발사 직후 총구 근처의 라이더/마운트 오폭 방지)
	if (const ADRCharacter* OtherCharacter = Cast<ADRCharacter>(OtherActor))
	{
		if (OtherCharacter->MountedOn.Get() == SourceAvatarActor) return;
	}
	if (const ADRRobotVacuumCharacter* OtherVacuum = Cast<ADRRobotVacuumCharacter>(OtherActor))
	{
		if (OtherVacuum->RiderOnTop.Get() == SourceAvatarActor) return;
	}

	// 시전자 옆에 선 아군은 스폰 순간부터 날(좌우로 넓다) 안에 있다. 총구를 떠나기도 전에 그 아군을
	// 맞히거나(아군 피해 칩) 날려 보내고(강화탄) 탄이 사라지지 않게, 스폰 프레임의 아군 오버랩은 넘긴다
	const bool bFriend = !UDRAbilitySystemLibrary::IsNotFriend(SourceAvatarActor, OtherActor);
	if (bFriend && GFrameCounter == SpawnFrame) return;

	// ★Plan5(공기탄칩)★ ① 아군도 적과 같은 피해 — 강화탄이면 넉백도 일반 피해 경로로 (D-94 · D-95).
	//   탑승 연결(내 위 탑승자 · 내가 탄 청소기)은 바로 위에서 이미 통과했다 (사용자 확정).
	//   ★아래 "강화탄 아군 = 넉백만" 분기보다 앞★ 이어야 강화탄도 아군에게 피해를 준다 (T-107)
	if (bHitsAllies && bFriend)
	{
		if (!bHit) OnHit();
		if (HasAuthority())
		{
			ApplyImpactDamage(OtherActor);
			Destroy();
		}
		else
		{
			bHit = true;
		}
		return;
	}

	// 강화탄 + 아군 플레이어 → 데미지 없이 넉백만 (부모 파이프라인은 IsNotFriend에서 아군을 걸러버림)
	if (bEnhanced && bFriend)
	{
		if (!bHit) OnHit();

		if (HasAuthority())
		{
			if (ACharacter* Ally = Cast<ACharacter>(OtherActor))
			{
				// 부모 넉백 공식과 동일: 진행 방향 기준 45도 상향
				FRotator KnockRotation = GetActorRotation();
				KnockRotation.Pitch = 45.f;
				const FVector KnockbackForce = KnockRotation.Vector() * DamageEffectParams.KnockbackForceMagnitude;
				Ally->LaunchCharacter(KnockbackForce, true, true);
			}
			Destroy();
		}
		else
		{
			bHit = true;
		}
		return;
	}

	// 적: 기존 파이프라인 (KnockbackChance 100 → AttributeSet에서 LaunchCharacter)
	Super::OnSphereOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}
