// Copyright DaeRune


#include "Actor/DRVacuumAirProjectile.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Character/DRCharacter.h"
#include "Character/DRRobotVacuumCharacter.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

void ADRVacuumAirProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRVacuumAirProjectile, bEnhanced);
	DOREPLIFETIME(ADRVacuumAirProjectile, bFading);
}

void ADRVacuumAirProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 부모의 LifeSpan 자동 소멸을 취소하고 자체 페이드 타이머 사용 (Plan3 §6.2)
	SetLifeSpan(0.f);

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

	// 강화탄 + 아군 플레이어 → 데미지 없이 넉백만 (부모 파이프라인은 IsNotFriend에서 아군을 걸러버림)
	if (bEnhanced && !UDRAbilitySystemLibrary::IsNotFriend(SourceAvatarActor, OtherActor))
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
